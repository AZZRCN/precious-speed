// 喵喵喵~ https://space.bilibili.com/620657947
// AZZRCN
// https://github.com/AZZRCN
// ============================ LC 回执 ============================
// Submission #391745  2026/8/10 14:40:25  C++23  AC  9 ms  11.07 MiB
//   计分点 small_00 = 9ms (T=160000 组短数, I/O + 解析 bound)
//   大数点全部压到 5ms 平台 = 纯 I/O 地板, 算术已非瓶颈
//   本地预测 maxCyc 19.5M ~ 6.7ms  ->  实测 9ms (偏差 +34%)
//   逐点明细: archive/submissions/391745_add_v2_9ms_AC.md
// =================================================================
// HEX addition v2 — small-case optimized.
// 关键: LC 计分取最长测试点, add 的最长点是 small_00 (T=160000 个 <=18 位小数).
//  - AVX2 一次 32 字节找分隔符 (替代逐字节扫描)
//  - 掩码化 SSE 解析: 任意 L<=16 位定长 9 条指令 (替代标量逐位循环)
//  - <=32 位走 __uint128_t 快路径 (覆盖 small/carry_chain 绝大多数 case)
//  - 无分支格式化: 33 位右对齐临时区 + 单次 32B 拷贝 (替代逐位 while 循环)
// (禁用: LC 对 optimize pragma 会 CE)
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <unistd.h>

static constexpr int PAD    = 128;
static constexpr int INCAP  = 9 << 20;
static constexpr int OUTCAP = 9 << 20;
static constexpr int MAXC   = 100010;      // 1.6M hex / 16 = 100000 limbs

alignas(64) static char inbuf_[PAD + INCAP + PAD];
static char* const inbuf = inbuf_ + PAD;
alignas(64) static char outbuf[OUTCAP + 128];
static uint64_t A[MAXC + 4], B[MAXC + 4], R[MAXC + 4];

// ---- masks: MT.m[L] 保留末尾 L 字节 ----
struct MaskTab {
    uint8_t m[17][16];
    constexpr MaskTab() : m{} {
        for (int L = 0; L <= 16; ++L)
            for (int i = 0; i < 16; ++i) m[L][i] = (i >= 16 - L) ? 0xFF : 0x00;
    }
};
alignas(64) static constexpr MaskTab MT{};

static inline __m128i a2n(__m128i v) {
    __m128i s0 = _mm_sub_epi8(v, _mm_set1_epi8('0'));
    __m128i sa = _mm_sub_epi8(_mm_or_si128(v, _mm_set1_epi8(0x20)),
                              _mm_set1_epi8('a' - 10));
    __m128i gt = _mm_cmpgt_epi8(s0, _mm_set1_epi8(9));
    return _mm_blendv_epi8(s0, sa, gt);
}

// 末尾 16 个 hex 字符 [end-16, end) -> u64
static inline uint64_t hexfull(const char* end) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((uint64_t)_mm_cvtsi128_si64(b));
}
// 末尾 L(<=16) 个 hex 字符 [end-L, end) -> u64 (定长无分支)
static inline uint64_t hexpart(const char* end, int L) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    nib = _mm_and_si128(nib, _mm_load_si128((const __m128i*)MT.m[L]));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((uint64_t)_mm_cvtsi128_si64(b));
}
static inline void hex16_store(uint64_t val, char* out) {
    val = __builtin_bswap64(val);
    __m128i v   = _mm_cvtsi64_si128((long long)val);
    __m128i hi  = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
    __m128i lo  = _mm_and_si128(v, _mm_set1_epi8(0x0F));
    __m128i nib = _mm_unpacklo_epi8(hi, lo);
    __m128i gt9 = _mm_cmpgt_epi8(nib, _mm_set1_epi8(9));
    __m128i asc = _mm_add_epi8(nib, _mm_set1_epi8('0'));
    __m128i alp = _mm_add_epi8(nib, _mm_set1_epi8('A' - 10));
    _mm_storeu_si128((__m128i*)out,
                     _mm_or_si128(_mm_andnot_si128(gt9, asc),
                                  _mm_and_si128(gt9, alp)));
}

// token 长度 (连续 > ' ' 的字节数)
static inline int tok_len(const char* p) {
    const __m256i sp = _mm256_set1_epi8(' ');
    __m256i v = _mm256_loadu_si256((const __m256i*)p);
    uint32_t m = ~(uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
    if (m) return (int)_tzcnt_u32(m);
    int n = 32;
    for (;;) {
        v = _mm256_loadu_si256((const __m256i*)(p + n));
        m = ~(uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
        if (m) return n + (int)_tzcnt_u32(m);
        n += 32;
    }
}

// 向量化跳过连续空白 (>= ' ' 的字节视为分隔符), 返回首个非空白位置
// 边界安全：inbuf 末尾有 96B 零填充('\0'<' '=空白), 故 SIMD 越界读到零区也安全。
static inline const char* skip_spaces(const char* p) {
    const __m256i sp = _mm256_set1_epi8(' ');
    for (;;) {
        __m256i v = _mm256_loadu_si256((const __m256i*)p);
        uint32_t m = (uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));  // 1=非空白
        if (m) return p + (int)_tzcnt_u32(m);
        p += 32;
    }
}

// 解析任意长度 -> 小端 limb 数组, 返回 limb 数
static inline int parse_limbs(const char* s, int n, uint64_t* out) {
    int nc = (n + 15) >> 4;
    const char* end = s + n;
    int full = nc - 1;
    for (int c = 0; c < full; ++c) out[c] = hexfull(end - (c << 4));
    int rem = n - (full << 4);
    out[full] = hexpart(s + rem, rem);
    return nc;
}

static inline int mag_cmp(const uint64_t* a, int na, const uint64_t* b, int nb) {
    if (na != nb) return na > nb ? 1 : -1;
    for (int i = na - 1; i >= 0; --i)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}

int main() {
    int len = 0;
    for (;;) {
        long r = read(0, inbuf + len, INCAP - len);
        if (r <= 0) break;
        len += (int)r;
    }
    memset(inbuf + len, 0, 96);
    inbuf[len] = '\n';

    const char* p = inbuf;
    while (*p < '0') ++p;
    uint32_t T = 0;
    while (*p > ' ') T = T * 10 + (uint32_t)(*p++ - '0');

    char* out = outbuf;
    alignas(64) char tmp[128];

        for (uint32_t t = 0; t < T; ++t) {
            p = skip_spaces(p);
            int sa = (*p == '-'); p += sa;
            const char* a0 = p;
            int la = tok_len(p); p += la;
            p = skip_spaces(p);
            int sb = (*p == '-'); p += sb;
            const char* b0 = p;
            int lb = tok_len(p); p += lb;

        // ---------- 快路径 ----------
        // 拆 u64(<=16 位) / u128(17..32 位) 两档: <=16 位走纯 u64, 省掉 u128 拆分与 32B 冗余 avx 拷贝.
        if (la <= 16 && lb <= 16) {
            uint64_t av = hexpart(a0 + la, la);
            uint64_t bv = hexpart(b0 + lb, lb);
            uint64_t mag, topc = 0;
            int neg;
            if (sa == sb) { mag = av + bv; topc = (mag < av); neg = sa; }
            else if (av >= bv) { mag = av - bv; neg = sa; }
            else { mag = bv - av; neg = sb; }
            int isz = (mag == 0 && topc == 0);
            if (neg && !isz) *out++ = '-';
            if (isz) { *out++ = '0'; *out++ = '\n'; continue; }
            uint64_t bits = topc ? 65 : 64 - _lzcnt_u64(mag);
            int d = (int)((bits + 3) >> 2);            // 1..17
            if (topc) {                                 // 17 位: '1' + lo(16 位)
                *out++ = '1';
                hex16_store(mag, out); out += 16;
            } else {                                     // <=16 位: 16B 滑窗存 (尾随字节被下条覆盖/不flush, 同原设计)
                hex16_store(mag, tmp);
                _mm_storeu_si128((__m128i*)out, _mm_loadu_si128((const __m128i*)(tmp + 16 - d)));
                out += d;
            }
            *out++ = '\n';
            continue;
        }
        if (la <= 32 && lb <= 32) {
            // 17..32 位: 原 u128 算术, 输出仅拷 d 字节 (去 32B 冗余 avx 拷贝)
            __uint128_t av, bv;
            if (la <= 16) av = hexpart(a0 + la, la);
            else av = ((__uint128_t)hexpart(a0 + la - 16, la - 16) << 64) | hexfull(a0 + la);
            if (lb <= 16) bv = hexpart(b0 + lb, lb);
            else bv = ((__uint128_t)hexpart(b0 + lb - 16, lb - 16) << 64) | hexfull(b0 + lb);

            __uint128_t mag;
            uint64_t topc = 0;
            int neg;
            if (sa == sb) { mag = av + bv; topc = (mag < av); neg = sa; }
            else if (av >= bv) { mag = av - bv; neg = sa; }
            else { mag = bv - av; neg = sb; }

            uint64_t hi = (uint64_t)(mag >> 64), lo = (uint64_t)mag;
            uint64_t bits = 64 - _lzcnt_u64(lo);
            if (hi) bits = 128 - _lzcnt_u64(hi);
            if (topc) bits = 129;
            int d = (int)((bits + 3) >> 2);
            d += (d == 0);
            int isz = (bits == 0);
            if (neg && !isz) *out++ = '-';
            if (isz) { *out++ = '0'; *out++ = '\n'; continue; }
            if (topc) {                                 // 129 位: '1' + 32 位
                *out++ = '1';
                hex16_store(hi, out); hex16_store(lo, out + 16); out += 32;
            } else {
                hex16_store(hi, tmp); hex16_store(lo, tmp + 16);
                _mm256_storeu_si256((__m256i*)out, _mm256_loadu_si256((const __m256i*)(tmp + 32 - d)));
                out += d;
            }
            *out++ = '\n';
            continue;
        }

        // ---------- 通用路径: limb ----------
        int nca = parse_limbs(a0, la, A);
        int ncb = parse_limbs(b0, lb, B);
        while (nca > 1 && A[nca - 1] == 0) --nca;
        while (ncb > 1 && B[ncb - 1] == 0) --ncb;

        int ncr, rsign;
        if (sa == sb) {
            int minc = nca < ncb ? nca : ncb;
            int maxc = nca > ncb ? nca : ncb;
            const uint64_t* L = (nca >= ncb) ? A : B;
            unsigned char c = 0;
            int i = 0;
            for (; i < minc; ++i)
                c = _addcarry_u64(c, A[i], B[i], (unsigned long long*)&R[i]);
            for (; i < maxc; ++i)
                c = _addcarry_u64(c, L[i], 0, (unsigned long long*)&R[i]);
            if (c) R[maxc++] = 1;
            ncr = maxc; rsign = sa;
        } else {
            int cmp = mag_cmp(A, nca, B, ncb);
            if (cmp == 0) { *out++ = '0'; *out++ = '\n'; continue; }
            const uint64_t* big   = cmp > 0 ? A : B;
            const uint64_t* small = cmp > 0 ? B : A;
            int nbig   = cmp > 0 ? nca : ncb;
            int nsmall = cmp > 0 ? ncb : nca;
            rsign = cmp > 0 ? sa : sb;
            unsigned char br = 0;
            int i = 0;
            for (; i < nsmall; ++i)
                br = _subborrow_u64(br, big[i], small[i], (unsigned long long*)&R[i]);
            for (; i < nbig; ++i)
                br = _subborrow_u64(br, big[i], 0, (unsigned long long*)&R[i]);
            ncr = nbig;
        }
        while (ncr > 1 && R[ncr - 1] == 0) --ncr;

        if (rsign && !(ncr == 1 && R[0] == 0)) *out++ = '-';
        uint64_t top = R[ncr - 1];
        uint64_t tb = 64 - _lzcnt_u64(top);
        int d = (int)((tb + 3) >> 2); d += (d == 0);
        hex16_store(top << (64 - 4 * d), out);
        out += d;
        for (int c = ncr - 2; c >= 0; --c) { hex16_store(R[c], out); out += 16; }
        *out++ = '\n';
    }

    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    return 0;
}
