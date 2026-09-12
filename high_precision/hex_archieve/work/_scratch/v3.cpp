// HEX addition v3 — 在 v2 基础上消除 small_00 的分支开销
//  1) 有符号 __int128 快路径: 消除 sa==sb / av>=bv 两个 50% 误预测分支
//  2) 单次 64 字节扫描同时定位 A、B 两个 token (替代 2×tok_len + 2×空白跳过循环)
//  3) 无分支双 chunk 解析 (L<=31 恒定 2 次 hexpart/操作数)
#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <unistd.h>

static constexpr int PAD    = 128;
static constexpr int INCAP  = 9 << 20;
static constexpr int OUTCAP = 9 << 20;
static constexpr int MAXC   = 100010;

alignas(64) static char inbuf_[PAD + INCAP + PAD];
alignas(64) static char outbuf[OUTCAP + 128];
static char* const inbuf = inbuf_ + PAD;
static uint64_t A[MAXC + 4], B[MAXC + 4], R[MAXC + 4];

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
static inline uint64_t hexfull(const char* end) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((uint64_t)_mm_cvtsi128_si64(b));
}
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
    while (*p <= ' ') ++p;

    char* out = outbuf;
    alignas(64) char tmp[64];
    const __m256i SP = _mm256_set1_epi8(' ');

    for (uint32_t t = 0; t < T; ++t) {
        // ---- 单次 64B 扫描定位 A、B ----
        __m256i v0 = _mm256_loadu_si256((const __m256i*)p);
        __m256i v1 = _mm256_loadu_si256((const __m256i*)(p + 32));
        uint64_t nsep = (uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v0, SP))
                      | ((uint64_t)(uint32_t)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v1, SP)) << 32);
        uint64_t sep = ~nsep;
        // _bzhi(~0,n) 在 n>=64 时原样返回, 取反得 0 -> tzcnt 得 64, 天然安全
        uint64_t ea = _tzcnt_u64(sep);                                  // A token 结束偏移
        uint64_t sb = _tzcnt_u64(nsep & ~_bzhi_u64(~0ULL, ea));         // B token 起始偏移
        uint64_t eb = _tzcnt_u64(sep & ~_bzhi_u64(~0ULL, sb));          // B token 结束偏移

        if (__builtin_expect(eb < 64, 1)) {
            int nga = (p[0] == '-');
            int ngb = (p[sb] == '-');
            const char* aend = p + ea;
            const char* bend = p + eb;
            int la = (int)ea - nga;
            int lb = (int)(eb - sb) - ngb;
            if (__builtin_expect((la | lb) <= 31, 1)) {
                int la1 = la < 16 ? la : 16, la2 = la - la1;
                int lb1 = lb < 16 ? lb : 16, lb2 = lb - lb1;
                __uint128_t av = ((__uint128_t)hexpart(aend - la1, la2) << 64)
                               | hexpart(aend, la1);
                __uint128_t bv = ((__uint128_t)hexpart(bend - lb1, lb2) << 64)
                               | hexpart(bend, lb1);
                __int128 ma = -(__int128)nga, mb = -(__int128)ngb;
                __int128 x = ((__int128)av ^ ma) - ma;
                __int128 y = ((__int128)bv ^ mb) - mb;
                __int128 s = x + y;
                __int128 msk = s >> 127;
                __uint128_t mag = (__uint128_t)((s ^ msk) - msk);
                int neg = (int)(msk & 1);

                uint64_t hi = (uint64_t)(mag >> 64), lo = (uint64_t)mag;
                uint64_t bits = 64 - _lzcnt_u64(lo);
                if (hi) bits = 128 - _lzcnt_u64(hi);
                int d = (int)((bits + 3) >> 2);
                d += (d == 0);

                *out = '-'; out += neg;
                hex16_store(hi, tmp);
                hex16_store(lo, tmp + 16);
                _mm256_storeu_si256((__m256i*)out,
                    _mm256_loadu_si256((const __m256i*)(tmp + 32 - d)));
                out += d;
                *out++ = '\n';
                p += eb;
                while (*p <= ' ') ++p;
                continue;
            }
            // 超长操作数 -> 落到通用路径 (p 未推进, 重新扫描)
        }

        // ---------- 通用路径 ----------
        {
            while (*p <= ' ') ++p;
            int nga = (*p == '-'); p += nga;
            const char* a0 = p;
            int la = tok_len(p); p += la;
            while (*p <= ' ') ++p;
            int ngb = (*p == '-'); p += ngb;
            const char* b0 = p;
            int lb = tok_len(p); p += lb;

            int nca = parse_limbs(a0, la, A);
            int ncb = parse_limbs(b0, lb, B);
            while (nca > 1 && A[nca - 1] == 0) --nca;
            while (ncb > 1 && B[ncb - 1] == 0) --ncb;

            int ncr, rsign;
            if (nga == ngb) {
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
                ncr = maxc; rsign = nga;
            } else {
                int cmp = mag_cmp(A, nca, B, ncb);
                if (cmp == 0) { *out++ = '0'; *out++ = '\n'; goto nextcase; }
                const uint64_t* big   = cmp > 0 ? A : B;
                const uint64_t* small = cmp > 0 ? B : A;
                int nbig   = cmp > 0 ? nca : ncb;
                int nsmall = cmp > 0 ? ncb : nca;
                rsign = cmp > 0 ? nga : ngb;
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
    nextcase:;
    }

    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    return 0;
}
