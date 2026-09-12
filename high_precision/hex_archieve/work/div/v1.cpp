// HEX division v1  (A>=0, B>0, floor 除法, 输出 "q r")
//  I/O   : read(2) 整读 + AVX2 分词 + 掩码化 SSE hex 解析 + 一次 write(2)
//  tier1 : la,lb<=16      -> u64 除法      (覆盖 small/medium_00/r_nearly_zero_00)
//  tier2 : la<=32,lb<=16  -> u128/u64
//  tier3 : nb==1          -> divrem_1
//  tier4 : 通用           -> Knuth Algorithm D (base 2^64)
// 注意: 绝不使用 #pragma GCC optimize —— LC 会 CE。只允许 target。
#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <unistd.h>

using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;

static constexpr int PAD = 128;
static constexpr int INCAP = 9 << 20;
static constexpr int OUTCAP = 10 << 20;
static constexpr int MAXC = 100010;   // 1.6M hex / 16

alignas(64) static char inbuf_[PAD + INCAP + PAD];
alignas(64) static char outbuf[OUTCAP + 256];
static char* const inbuf = inbuf_ + PAD;
static u64 A[MAXC + 8], B[MAXC + 8], Q[MAXC + 8], R[MAXC + 8];
static u64 AN[MAXC + 8], BN[MAXC + 8];

// ============================ hex I/O ============================
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
    __m128i sa = _mm_sub_epi8(_mm_or_si128(v, _mm_set1_epi8(0x20)), _mm_set1_epi8('a' - 10));
    __m128i gt = _mm_cmpgt_epi8(s0, _mm_set1_epi8(9));
    return _mm_blendv_epi8(s0, sa, gt);
}
static inline u64 hexfull(const char* end) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((u64)_mm_cvtsi128_si64(b));
}
static inline u64 hexpart(const char* end, int L) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    nib = _mm_and_si128(nib, _mm_load_si128((const __m128i*)MT.m[L]));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((u64)_mm_cvtsi128_si64(b));
}
static inline void hex16_store(u64 val, char* out) {
    val = __builtin_bswap64(val);
    __m128i v = _mm_cvtsi64_si128((long long)val);
    __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
    __m128i lo = _mm_and_si128(v, _mm_set1_epi8(0x0F));
    __m128i nib = _mm_unpacklo_epi8(hi, lo);
    __m128i gt9 = _mm_cmpgt_epi8(nib, _mm_set1_epi8(9));
    __m128i asc = _mm_add_epi8(nib, _mm_set1_epi8('0'));
    __m128i alp = _mm_add_epi8(nib, _mm_set1_epi8('A' - 10));
    _mm_storeu_si128((__m128i*)out, _mm_or_si128(_mm_andnot_si128(gt9, asc), _mm_and_si128(gt9, alp)));
}
static inline int tok_len(const char* p) {
    const __m256i sp = _mm256_set1_epi8(' ');
    __m256i v = _mm256_loadu_si256((const __m256i*)p);
    u32 m = ~(u32)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
    if (m) return (int)_tzcnt_u32(m);
    int n = 32;
    for (;;) {
        v = _mm256_loadu_si256((const __m256i*)(p + n));
        m = ~(u32)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
        if (m) return n + (int)_tzcnt_u32(m);
        n += 32;
    }
}
static inline int parse_limbs(const char* s, int n, u64* out) {
    int nc = (n + 15) >> 4;
    const char* end = s + n;
    int full = nc - 1;
    for (int c = 0; c < full; ++c) out[c] = hexfull(end - (c << 4));
    int rem = n - (full << 4);
    out[full] = hexpart(s + rem, rem);
    return nc;
}
// 写单个 u64 (无前导零), 返回新位置
static inline char* put_u64(char* out, u64 v) {
    int d = v ? 16 - (int)(_lzcnt_u64(v) >> 2) : 1;
    hex16_store(v << (64 - 4 * d), out);
    return out + d;
}
static inline char* put_big(char* out, const u64* V, int n) {
    while (n > 1 && V[n - 1] == 0) --n;
    if (n <= 0 || (n == 1 && V[0] == 0)) { *out++ = '0'; return out; }
    u64 top = V[n - 1];
    int d = 16 - (int)(_lzcnt_u64(top) >> 2);
    hex16_store(top << (64 - 4 * d), out);
    out += d;
    for (int c = n - 2; c >= 0; --c) { hex16_store(V[c], out); out += 16; }
    return out;
}

// ============================ division cores ============================
// A / d, d 单 limb。q 长度 na (可能有前导 0)，返回余数。
static u64 divrem_1(const u64* a, int na, u64 d, u64* q) {
    if (d == 1) { std::memcpy(q, a, (size_t)na * 8); return 0; }
    u64 rem = 0;
    for (int i = na - 1; i >= 0; --i) {
        u128 cur = ((u128)rem << 64) | a[i];
        q[i] = (u64)(cur / d);
        rem = (u64)(cur % d);
    }
    return rem;
}

// Knuth Algorithm D。U: mn limbs, V: n limbs (n>=2, V[n-1]!=0, mn>=n)
// Q: mn-n+1 limbs, R: n limbs
static void knuthD(const u64* U, int mn, const u64* V, int n, u64* Qout, u64* Rout) {
    const int m = mn - n;
    const int s = (int)_lzcnt_u64(V[n - 1]);
    if (s) {
        for (int i = n - 1; i > 0; --i) BN[i] = (V[i] << s) | (V[i - 1] >> (64 - s));
        BN[0] = V[0] << s;
        for (int i = mn - 1; i > 0; --i) AN[i] = (U[i] << s) | (U[i - 1] >> (64 - s));
        AN[0] = U[0] << s;
        AN[mn] = U[mn - 1] >> (64 - s);
    } else {
        std::memcpy(BN, V, (size_t)n * 8);
        std::memcpy(AN, U, (size_t)mn * 8);
        AN[mn] = 0;
    }
    const u64 vn1 = BN[n - 1], vn2 = BN[n - 2];

    for (int j = m; j >= 0; --j) {
        const u128 num = ((u128)AN[j + n] << 64) | AN[j + n - 1];
        u64 qhat, rhat;
        bool refine = true;
        if (AN[j + n] >= vn1) {
            qhat = ~0ULL;
            u128 t = num - (u128)qhat * vn1;
            if (t >> 64) refine = false;
            else rhat = (u64)t;
        } else {
            qhat = (u64)(num / vn1);
            rhat = (u64)(num - (u128)qhat * vn1);
        }
        if (refine) {
            while ((u128)qhat * vn2 > (((u128)rhat << 64) | AN[j + n - 2])) {
                --qhat;
                rhat += vn1;
                if (rhat < vn1) break;   // rhat 溢出 2^64, 停止修正
            }
        }
        // AN[j..j+n] -= qhat * BN[0..n)
        u64 carry = 0, borrow = 0;
        for (int i = 0; i < n; ++i) {
            u128 pr = (u128)qhat * BN[i] + carry;
            carry = (u64)(pr >> 64);
            u64 sub = (u64)pr;
            u64 cur = AN[i + j];
            u64 d1 = cur - sub;
            u64 nb = (cur < sub);
            u64 d2 = d1 - borrow;
            nb += (d1 < borrow);
            AN[i + j] = d2;
            borrow = nb;
        }
        {
            u64 cur = AN[j + n];
            u64 d1 = cur - carry;
            u64 nb = (cur < carry);
            u64 d2 = d1 - borrow;
            nb += (d1 < borrow);
            AN[j + n] = d2;
            borrow = nb;
        }
        if (borrow) {           // qhat 大了 1，把 B 加回去
            --qhat;
            unsigned char c = 0;
            for (int i = 0; i < n; ++i)
                c = _addcarry_u64(c, AN[i + j], BN[i], (unsigned long long*)&AN[i + j]);
            AN[j + n] += c;
        }
        Qout[j] = qhat;
    }
    if (s) {
        for (int i = 0; i < n - 1; ++i) Rout[i] = (AN[i] >> s) | (AN[i + 1] << (64 - s));
        Rout[n - 1] = AN[n - 1] >> s;
    } else {
        std::memcpy(Rout, AN, (size_t)n * 8);
    }
}

static inline int mag_cmp(const u64* a, int na, const u64* b, int nb) {
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
    std::memset(inbuf + len, 0, 96);
    inbuf[len] = '\n';

    const char* p = inbuf;
    while (*p < '0') ++p;
    u32 T = 0;
    while (*p > ' ') T = T * 10 + (u32)(*p++ - '0');

    char* out = outbuf;

    for (u32 t = 0; t < T; ++t) {
        while (*p <= ' ') ++p;
        const char* a0 = p;
        int la = tok_len(p); p += la;
        while (*p <= ' ') ++p;
        const char* b0 = p;
        int lb = tok_len(p); p += lb;

        if (la <= 16 && lb <= 16) {
            // ---------- tier1 ----------
            u64 av = hexpart(a0 + la, la), bv = hexpart(b0 + lb, lb);
            u64 q = av / bv, r = av - q * bv;
            out = put_u64(out, q);
            *out++ = ' ';
            out = put_u64(out, r);
            *out++ = '\n';
            continue;
        }
        if (la <= 32 && lb <= 16) {
            // ---------- tier2 ----------
            u128 av = ((u128)hexpart(a0 + la - 16, la - 16) << 64) | hexfull(a0 + la);
            u64 bv = hexpart(b0 + lb, lb);
            u128 q = av / bv;
            u64 r = (u64)(av - q * bv);
            u64 qh = (u64)(q >> 64), ql = (u64)q;
            if (qh) { out = put_u64(out, qh); hex16_store(ql, out); out += 16; }
            else out = put_u64(out, ql);
            *out++ = ' ';
            out = put_u64(out, r);
            *out++ = '\n';
            continue;
        }

        // ---------- 通用 ----------
        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        while (na > 1 && A[na - 1] == 0) --na;
        while (nb > 1 && B[nb - 1] == 0) --nb;

        if (na == 1 && A[0] == 0) { *out++ = '0'; *out++ = ' '; *out++ = '0'; *out++ = '\n'; continue; }
        if (mag_cmp(A, na, B, nb) < 0) {           // A < B  =>  q=0, r=A
            *out++ = '0'; *out++ = ' ';
            out = put_big(out, A, na);
            *out++ = '\n';
            continue;
        }
        if (nb == 1) {
            u64 r = divrem_1(A, na, B[0], Q);
            out = put_big(out, Q, na);
            *out++ = ' ';
            out = put_u64(out, r);
            *out++ = '\n';
            continue;
        }
        knuthD(A, na, B, nb, Q, R);
        out = put_big(out, Q, na - nb + 1);
        *out++ = ' ';
        out = put_big(out, R, nb);
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
