// HEX division v2  (A>=0, B>0, floor 除法, 输出 "q r")
//  tier1 : la,lb<=16      -> u64 除法      (覆盖 small/medium_00/r_nearly_zero_00)
//  tier2 : la<=32,lb<=16  -> u128/u64
//  tier3 : nb==1          -> divrem_1
//  tier4 : 小规模         -> Knuth Algorithm D
//  tier5 : 大规模         -> Burnikel-Ziegler 递归除法 (叶子 Knuth D, 乘法走 AVX2 FFT)
// 注意: 绝不使用 #pragma GCC optimize —— LC 会 CE。只允许 target。
#pragma GCC target("avx2,fma,bmi,bmi2,popcnt,lzcnt")

#include <cstdint>
#include <cstring>
#include <cmath>
#include <complex>
#include <immintrin.h>
#include <unistd.h>

using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;

static constexpr int PAD = 128;
static constexpr int INCAP = 9 << 20;
static constexpr int OUTCAP = 10 << 20;
static constexpr int MAXC = 100010;          // 1.6M hex / 16
#ifndef BZ_CUTOFF
#define BZ_CUTOFF 64                         // BZ 叶子规模 (limbs)
#endif
#ifndef BZ_MIN
#define BZ_MIN (BZ_CUTOFF * 2 + 32)          // 低于此规模直接 Knuth D
#endif
#ifndef KD_QMAX
#define KD_QMAX 64                           // 商 limb 数 <= 此值时 Knuth D 完胜 BZ
#endif
#ifndef MULBF_MAX
#define MULBF_MAX 48                         // mulg: nb <= 此值走学校法
#endif
#ifndef BARRETT_T
#define BARRETT_T 3                          // 块数 >= 此值时用 Barrett (倒数摊销划算)
#endif

alignas(64) static char inbuf_[PAD + INCAP + PAD];
alignas(64) static char outbuf[OUTCAP + 256];
static char* const inbuf = inbuf_ + PAD;
static u64 A[MAXC + 8], B[MAXC + 8];
static u64 Qout[MAXC + 8], Rout[MAXC + 8];
static u64 AN[2 * MAXC + 16], BN[MAXC + 8];  // Knuth D 归一化缓冲
static u64 AS[MAXC + 8192], BS[MAXC + 8192]; // BZ 顶层 shift 缓冲 (块长 n 可略大于 nb)
static u64 VB[MAXC + 8192];                  // Barrett 倒数 V 的低 n limbs (V = 2^64n + VB)
static u64 WORK[3200000];                    // BZ/Barrett 工作栈 (~25.6MB), 峰值约 16*n
static u64* wp = WORK;

static constexpr u32 LMMAX = 1u << 20;
alignas(64) static double FB[LMMAX], GB[LMMAX];

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

// ============================ AVX2 FFT ============================
#define FFT_LEAF_LOG 11
namespace fft {
using cpx = __m128d;
static cpx* tw = nullptr;
static u32 twlen = 0;

static inline cpx cmul(cpx a, cpx b) {
    return _mm_fmaddsub_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline cpx cmulconj(cpx a, cpx b) {
    return _mm_fmsubadd_pd(_mm_unpacklo_pd(b, b), a, _mm_mul_pd(_mm_unpackhi_pd(b, b), _mm_permute_pd(a, 1)));
}
static inline cpx cmulspec(cpx a, cpx b) {
    return _mm_fmadd_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline cpx cscale(cpx a, double s) { return _mm_mul_pd(a, _mm_set1_pd(s)); }

static void resize(u32 n) {
    if (n <= (twlen << 1)) return;
    u32 halfLog = (u32)(31 - __builtin_clz(n)) >> 1, halfSize = 1u << halfLog;
    cpx* base = new cpx[(size_t)halfSize << 1];
    const double a0 = std::acos(-1.0) / halfSize, a1 = a0 / halfSize;
    for (u32 i = 0, j = (halfSize * 3) >> 1, p = 0; i != halfSize; p -= halfSize - (j >> __builtin_ctz(++i))) {
        int32_t sp = (int32_t)p;
        std::complex<double> f = std::polar(1.0, sp * a0), s = std::polar(1.0, sp * a1);
        base[i] = _mm_set_pd(f.imag(), f.real());
        base[i | halfSize] = _mm_set_pd(s.imag(), s.real());
    }
    cpx* nf = new cpx[n >> 1];
    if (twlen) std::memcpy(nf, tw, (size_t)twlen * 16);
    delete[] tw;
    tw = nf;
    for (u32 i = twlen; i != (n >> 1); ++i)
        tw[i] = cmul(base[i & (halfSize - 1)], base[halfSize | (i >> halfLog)]);
    delete[] base;
    twlen = n >> 1;
}
static inline void bfPlain(cpx* s, u32 bs) {
    cpx* e = s + bs; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, y));
    }
    if (p != e) { cpx x = *p, y = p[bs]; *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y); }
}
static inline void bfFwd(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wsw = _mm256_permute_pd(w256, 0x5);
    cpx* e = s + bs; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d ym = _mm256_fmaddsub_pd(_mm256_unpacklo_pd(y, y), w256,
                                        _mm256_mul_pd(_mm256_unpackhi_pd(y, y), wsw));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, ym));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, ym));
    }
    if (p != e) { cpx x = *p, y = cmul(p[bs], w); *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y); }
}
static inline void bfInv(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wlo = _mm256_unpacklo_pd(w256, w256), whi = _mm256_unpackhi_pd(w256, w256);
    cpx* e = s + bs; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d d = _mm256_sub_pd(x, y);
        __m256d o = _mm256_fmsubadd_pd(wlo, d, _mm256_mul_pd(whi, _mm256_permute_pd(d, 0x5)));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), o);
    }
    if (p != e) { cpx x = *p, y = p[bs]; *p = _mm_add_pd(x, y), p[bs] = cmulconj(_mm_sub_pd(x, y), w); }
}
static void difFlat(cpx* d, u32 n, u32 bb) {
    u32 bc = 1;
    for (u32 bs = n >> 1, st = n; bs; st = bs, bs >>= 1, bc <<= 1) {
        const u32 base = bb * bc; u32 j = 0; cpx* s = d;
        if (base == 0) { bfPlain(s, bs); j = 1, s += st; }
        for (; j != bc; ++j, s += st) bfFwd(s, bs, tw[base + j]);
    }
}
static void ditFlat(cpx* d, u32 n, u32 bb) {
    u32 bc = n >> 1;
    for (u32 bs = 1, st = 2; bs != n; bs = st, st <<= 1, bc >>= 1) {
        const u32 base = bb * bc; u32 j = 0; cpx* s = d;
        if (base == 0) { bfPlain(s, bs); j = 1, s += st; }
        for (; j != bc; ++j, s += st) bfInv(s, bs, tw[base + j]);
    }
}
static void difRec(cpx* d, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { difFlat(d, n, bb); return; }
    const u32 h = n >> 1;
    if (bb == 0) bfPlain(d, h); else bfFwd(d, h, tw[bb]);
    difRec(d, h, bb << 1);
    difRec(d + h, h, (bb << 1) | 1);
}
static void ditRec(cpx* d, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { ditFlat(d, n, bb); return; }
    const u32 h = n >> 1;
    ditRec(d, h, bb << 1);
    ditRec(d + h, h, (bb << 1) | 1);
    if (bb == 0) bfPlain(d, h); else bfInv(d, h, tw[bb]);
}
static void pointwise(cpx* F, cpx* G, u32 n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    F[0] = cscale(cmulspec(F[0], G[0]), nf);
    F[1] = cscale(cmul(F[1], G[1]), nf);
    const cpx cjm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), 0));
    const cpx ngm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), (int64_t)(1ull << 63)));
    for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        for (u32 f = bs, b = f + bs - 1; f != be; ++f, --b) {
            cpx Fc = _mm_xor_pd(F[b], cjm), Gc = _mm_xor_pd(G[b], cjm);
            cpx fe = _mm_add_pd(F[f], Fc), fo = _mm_sub_pd(F[f], Fc);
            cpx ge = _mm_add_pd(G[f], Gc), go = _mm_sub_pd(G[f], Gc);
            cpx t = (f & 1) ? _mm_xor_pd(tw[f >> 1], ngm) : tw[f >> 1];
            cpx pa = _mm_sub_pd(cmul(fe, ge), cmul(cmul(fo, go), t));
            cpx pb = _mm_add_pd(cmul(ge, fo), cmul(fe, go));
            F[f] = cscale(_mm_add_pd(pa, pb), sf);
            F[b] = _mm_xor_pd(cscale(_mm_sub_pd(pa, pb), sf), cjm);
        }
    }
}
}  // namespace fft

static void split_b2(const u64* src, double* g, size_t n, int k) {
    const u32* f = (const u32*)src;
    u64 tmp = f[0], msk = (u64(1) << k) - 1;
    size_t i = 1, j = 0;
    int w = 32;
    while (i < (n << 1)) {
        g[j++] = (double)(int64_t)(tmp & msk);
        tmp >>= k, w -= k;
        if (w < 32) { tmp |= u64(f[i++]) << w; w += 32; }
    }
    while (w > 0) { g[j++] = (double)(int64_t)(tmp & msk); tmp >>= k, w -= k; }
}
static void merge_b2(u64* f, const double* g, size_t n, int k) {
    size_t i = 0, j = 0;
    int w = 0;
    u128 tmp = 0;
    while (i < n) {
        while (w < 64) { tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }
        f[i++] = (u64)tmp;
        tmp >>= 64, w -= 64;
    }
}
static inline int pick_k(size_t u) {
    static const size_t gk[11] = {19ull << 4,  18ull << 6,  17ull << 8,  16ull << 10,
                                  15ull << 12, 14ull << 14, 13ull << 16, 12ull << 18,
                                  11ull << 20, 10ull << 22, ~size_t(0)};
    int i = 0;
    while (u > gk[i]) ++i;
    return 19 - i;
}
static void mul_bf(const u64* a, int na, const u64* b, int nb, u64* c) {
    std::memset(c, 0, (size_t)(na + nb) * 8);
    for (int i = 0; i < nb; ++i) {
        u64 carry = 0, bi = b[i];
        for (int j = 0; j < na; ++j) {
            u128 t = (u128)a[j] * bi + c[i + j] + carry;
            c[i + j] = (u64)t;
            carry = (u64)(t >> 64);
        }
        c[i + na] = carry;
    }
}
static void mul_fft(const u64* a, int na, const u64* b, int nb, u64* c) {
    const size_t u = (size_t)na + nb;
    const int k = pick_k(u);
    const u32 coeffs = (u32)(u * 64 / k) + 1;
    const u32 lm = 2u << (31 - __builtin_clz(coeffs));
    std::memset(FB, 0, (size_t)lm * 8);
    std::memset(GB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    split_b2(b, GB, nb, k);
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    fft::difRec((fft::cpx*)GB, ts, 0);
    fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    fft::ditRec((fft::cpx*)FB, ts, 0);
    merge_b2(c, FB, u, k);
}
// c[0..na+nb) = a*b
static void mulg(const u64* a, int na, const u64* b, int nb, u64* c) {
    if (na < nb) { const u64* t = a; a = b; b = t; int s = na; na = nb; nb = s; }
    if (nb <= MULBF_MAX) mul_bf(a, na, b, nb, c);
    else mul_fft(a, na, b, nb, c);
}

// ============================ 基本 limb 运算 ============================
static inline int cmpn(const u64* a, const u64* b, int n) {
    for (int i = n - 1; i >= 0; --i)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}
static inline u64 addn(u64* a, const u64* b, int n) {
    unsigned char c = 0;
    for (int i = 0; i < n; ++i) c = _addcarry_u64(c, a[i], b[i], (unsigned long long*)&a[i]);
    return c;
}
static inline u64 subn(u64* a, const u64* b, int n) {
    unsigned char c = 0;
    for (int i = 0; i < n; ++i) c = _subborrow_u64(c, a[i], b[i], (unsigned long long*)&a[i]);
    return c;
}

// ============================ Knuth D ============================
static void knuthD(const u64* U, int mn, const u64* V, int n, u64* Qo, u64* Ro) {
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
        u64 qhat, rhat = 0;
        bool refine = true;
        if (AN[j + n] >= vn1) {
            qhat = ~0ULL;
            u128 t = num - (u128)qhat * vn1;
            if (t >> 64) refine = false; else rhat = (u64)t;
        } else {
            qhat = (u64)(num / vn1);
            rhat = (u64)(num - (u128)qhat * vn1);
        }
        if (refine) {
            while ((u128)qhat * vn2 > (((u128)rhat << 64) | AN[j + n - 2])) {
                --qhat;
                rhat += vn1;
                if (rhat < vn1) break;
            }
        }
        u64 carry = 0, borrow = 0;
        for (int i = 0; i < n; ++i) {
            u128 pr = (u128)qhat * BN[i] + carry;
            carry = (u64)(pr >> 64);
            u64 sub = (u64)pr, cur = AN[i + j];
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
        if (borrow) {
            --qhat;
            unsigned char c = 0;
            for (int i = 0; i < n; ++i)
                c = _addcarry_u64(c, AN[i + j], BN[i], (unsigned long long*)&AN[i + j]);
            AN[j + n] += c;
        }
        Qo[j] = qhat;
    }
    if (s) {
        for (int i = 0; i < n - 1; ++i) Ro[i] = (AN[i] >> s) | (AN[i + 1] << (64 - s));
        Ro[n - 1] = AN[n - 1] >> s;
    } else {
        std::memcpy(Ro, AN, (size_t)n * 8);
    }
}

// ============================ Burnikel-Ziegler ============================
static void div_3n_2n(const u64* Ain, const u64* Bin, int n, u64* Q, u64* R);

// A: 2n limbs, B: n limbs (归一化: B[n-1] 最高位为 1), 且 A < B*2^(64n)
// Q: n limbs, R: n limbs
static void div_2n_1n(const u64* Ain, const u64* Bin, int n, u64* Q, u64* R) {
    if (n < BZ_CUTOFF || (n & 1)) {
        u64* Qt = wp; wp += n + 1;
        knuthD(Ain, 2 * n, Bin, n, Qt, R);
        std::memcpy(Q, Qt, (size_t)n * 8);
        wp = Qt;
        return;
    }
    const int n2 = n >> 1;
    u64* save = wp;
    u64* R1 = wp; wp += n;
    div_3n_2n(Ain + n2, Bin, n2, Q + n2, R1);      // 高 3*n2 limbs
    u64* T = wp; wp += 3 * n2;
    std::memcpy(T, Ain, (size_t)n2 * 8);
    std::memcpy(T + n2, R1, (size_t)n * 8);
    div_3n_2n(T, Bin, n2, Q, R);
    wp = save;
}

// A: 3n limbs, B: 2n limbs (归一化), 且 A < B*2^(64n)
// Q: n limbs, R: 2n limbs
static void div_3n_2n(const u64* Ain, const u64* Bin, int n, u64* Q, u64* R) {
    const u64* B1 = Bin + n;
    const u64* B0 = Bin;
    u64* save = wp;
    u64* R1 = wp; wp += n + 1;

    if (cmpn(Ain + 2 * n, B1, n) < 0) {
        div_2n_1n(Ain + n, B1, n, Q, R1);
        R1[n] = 0;
    } else {
        // A2 == B1  =>  Q = 2^(64n)-1,  R1 = A1 + B1
        for (int i = 0; i < n; ++i) Q[i] = ~0ULL;
        std::memcpy(R1, Ain + n, (size_t)n * 8);
        R1[n] = addn(R1, B1, n);
    }
    u64* D = wp; wp += 2 * n;
    mulg(Q, n, B0, n, D);
    u64* Rt = wp; wp += 2 * n + 1;
    std::memcpy(Rt, Ain, (size_t)n * 8);
    std::memcpy(Rt + n, R1, (size_t)(n + 1) * 8);
    u64 br = subn(Rt, D, 2 * n);
    Rt[2 * n] -= br;
    while ((int64_t)Rt[2 * n] < 0) {
        for (int i = 0; i < n; ++i) if (Q[i]--) break;
        u64 c = addn(Rt, Bin, 2 * n);
        Rt[2 * n] += c;
    }
    std::memcpy(R, Rt, (size_t)(2 * n) * 8);
    wp = save;
}

// 顶层 BZ: A(na) / B(nb) -> Q(na-nb+1), R(nb)。要求 na >= nb >= BZ_MIN。
static void bz_divide(const u64* Ai, int na, const u64* Bi, int nb, u64* Q, u64* R) {
    // 块长 n = j*m，m 为 2 的幂，保证递归能一路二分到 j
    int q = nb / BZ_CUTOFF;
    int m = 1 << (32 - __builtin_clz((u32)(q ? q : 1)));
    int j = (nb + m - 1) / m;
    int n = j * m;                       // n >= nb
    const int sigma = 64 * n - (64 * nb - (int)_lzcnt_u64(Bi[nb - 1]));
    const int sw = sigma >> 6, sb = sigma & 63;   // limb 位移 + 位内位移

    // BS = B << sigma  (n limbs, 最高位为 1)
    std::memset(BS, 0, (size_t)n * 8);
    if (sb) {
        u64 carry = 0;
        for (int i = 0; i < nb; ++i) { BS[sw + i] = (Bi[i] << sb) | carry; carry = Bi[i] >> (64 - sb); }
        if (sw + nb < n) BS[sw + nb] = carry;
    } else {
        std::memcpy(BS + sw, Bi, (size_t)nb * 8);
    }

    // AS = A << sigma  (nas limbs)
    int nas = na + sw + 1;
    std::memset(AS, 0, (size_t)nas * 8);
    if (sb) {
        u64 carry = 0;
        for (int i = 0; i < na; ++i) { AS[sw + i] = (Ai[i] << sb) | carry; carry = Ai[i] >> (64 - sb); }
        AS[sw + na] = carry;
    } else {
        std::memcpy(AS + sw, Ai, (size_t)na * 8);
    }
    while (nas > 1 && AS[nas - 1] == 0) --nas;

    // 块数 t: 按 bit 长度算, 多留 1 bit 保证最高块 a_{t-1} < BS  (BZ 论文 step 5)
    // t = floor(bitlen(AS) / (64n)) + 1
    const int64_t abits = (int64_t)64 * (nas - 1) + (64 - (int)_lzcnt_u64(AS[nas - 1]));
    int t = (int)(abits / ((int64_t)64 * n)) + 1;
    if (t < 2) t = 2;

    // Z = AS 的最高两块 (2n limbs)，不足处补 0
    u64* Z = wp; wp += 2 * n + 8;
    u64* Qi = wp; wp += n + 8;
    std::memset(Q, 0, (size_t)(na - nb + 1) * 8);

    auto load_block = [&](int idx, u64* dst) {          // AS 的第 idx 块 (n limbs)
        int off = idx * n;
        int cnt = nas - off;
        if (cnt > n) cnt = n;
        if (cnt <= 0) { std::memset(dst, 0, (size_t)n * 8); return; }
        std::memcpy(dst, AS + off, (size_t)cnt * 8);
        if (cnt < n) std::memset(dst + cnt, 0, (size_t)(n - cnt) * 8);
    };

    if (t >= BARRETT_T) {
        // ---- Barrett: 预计算 V = floor((2^(128n)-1)/BS) = 2^(64n) + VB, 每块只需 2 次 n*n 乘法 ----
        u64* q2 = VB;
        {
            u64* Zv = wp; wp += 2 * n + 8;
            u64* rj = wp; wp += n + 8;
            for (int i = 0; i < n; ++i) Zv[i] = ~0ULL;      // 低半 = 2^(64n)-1
            for (int i = 0; i < n; ++i) Zv[n + i] = ~BS[i]; // 高半 = (2^(64n)-1) - BS  < BS
            div_2n_1n(Zv, BS, n, q2, rj);
            wp = Zv;
        }
        u64* P = wp; wp += 2 * n + 8;
        load_block(t - 1, Z + n);
        load_block(t - 2, Z);
        for (int i = t - 2; i >= 0; --i) {
            u64* Z1 = Z + n;                                // 高半 (= 上轮余数, < BS)
            mulg(Z1, n, q2, n, P);                          // P = Z1 * q2
            unsigned char c = 0;                            // qhat = Z1 + hi_n(Z1*q2)
            for (int k = 0; k < n; ++k)
                c = _addcarry_u64(c, Z1[k], P[n + k], (unsigned long long*)&Qi[k]);
            mulg(Qi, n, BS, n, P);                          // P = qhat * BS
            unsigned char br = 0;                           // Z -= P   (2n limbs)
            for (int k = 0; k < 2 * n; ++k)
                br = _subborrow_u64(br, Z[k], P[k], (unsigned long long*)&Z[k]);
            for (;;) {                                      // 修正: 至多 ~6 轮
                bool ge = false;
                for (int k = 2 * n - 1; k >= n; --k) if (Z[k]) { ge = true; break; }
                if (!ge && cmpn(Z, BS, n) >= 0) ge = true;
                if (!ge) break;
                unsigned char b2 = 0;
                for (int k = 0; k < n; ++k)
                    b2 = _subborrow_u64(b2, Z[k], BS[k], (unsigned long long*)&Z[k]);
                for (int k = n; b2; ++k) { u64 cur = Z[k]; Z[k] = cur - b2; b2 = (cur < b2); }
                for (int k = 0; k < n; ++k) if (++Qi[k]) break;
            }
            {
                int off = i * n;
                int lim = na - nb + 1 - off;
                if (lim > n) lim = n;
                if (lim > 0) std::memcpy(Q + off, Qi, (size_t)lim * 8);
            }
            std::memcpy(Z + n, Z, (size_t)n * 8);           // 余数搬到高半, 供下一轮
            if (i > 0) load_block(i - 1, Z);
        }
    } else {
        load_block(t - 1, Z + n);
        load_block(t - 2, Z);
        for (int i = t - 2; i > 0; --i) {
            div_2n_1n(Z, BS, n, Qi, Z + n);                 // 余数直接落回 Z 的高半区
            // Q += Qi << (64*n*i)
            {
                int off = i * n;
                int lim = na - nb + 1 - off;
                if (lim > n) lim = n;
                if (lim > 0) std::memcpy(Q + off, Qi, (size_t)lim * 8);
            }
            load_block(i - 1, Z);
        }
        div_2n_1n(Z, BS, n, Qi, Z + n);
        {
            int lim = na - nb + 1;
            if (lim > n) lim = n;
            std::memcpy(Q, Qi, (size_t)lim * 8);
        }
    }
    // R = (Z 高半区) >> sigma
    const u64* Rs = Z + n;
    {
        u64* dst = R;
        if (sb) {
            for (int i = 0; i < nb; ++i) {
                int k2 = i + sw;
                u64 lo = (k2 < n) ? (Rs[k2] >> sb) : 0;
                u64 hi = (k2 + 1 < n) ? (Rs[k2 + 1] << (64 - sb)) : 0;
                dst[i] = lo | hi;
            }
        } else {
            for (int i = 0; i < nb; ++i) dst[i] = (i + sw < n) ? Rs[i + sw] : 0;
        }
    }
    wp = Z;
}

static inline int mag_cmp(const u64* a, int na, const u64* b, int nb) {
    if (na != nb) return na > nb ? 1 : -1;
    return cmpn(a, b, na);
}

// ============================ divrem_1 ============================
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
            u64 av = hexpart(a0 + la, la), bv = hexpart(b0 + lb, lb);
            u64 qq = av / bv, rr = av - qq * bv;
            out = put_u64(out, qq);
            *out++ = ' ';
            out = put_u64(out, rr);
            *out++ = '\n';
            continue;
        }
        if (la <= 32 && lb <= 16) {
            u128 av = ((u128)hexpart(a0 + la - 16, la - 16) << 64) | hexfull(a0 + la);
            u64 bv = hexpart(b0 + lb, lb);
            u128 qq = av / bv;
            u64 rr = (u64)(av - qq * bv);
            u64 qh = (u64)(qq >> 64), ql = (u64)qq;
            if (qh) { out = put_u64(out, qh); hex16_store(ql, out); out += 16; }
            else out = put_u64(out, ql);
            *out++ = ' ';
            out = put_u64(out, rr);
            *out++ = '\n';
            continue;
        }

        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        while (na > 1 && A[na - 1] == 0) --na;
        while (nb > 1 && B[nb - 1] == 0) --nb;

        if (na == 1 && A[0] == 0) { *out++ = '0'; *out++ = ' '; *out++ = '0'; *out++ = '\n'; continue; }
        if (mag_cmp(A, na, B, nb) < 0) {
            *out++ = '0'; *out++ = ' ';
            out = put_big(out, A, na);
            *out++ = '\n';
            continue;
        }
        if (nb == 1) {
            u64 rr = divrem_1(A, na, B[0], Qout);
            out = put_big(out, Qout, na);
            *out++ = ' ';
            out = put_u64(out, rr);
            *out++ = '\n';
            continue;
        }
        // 商 limb 数 = na-nb+1。极短商时 Knuth D 只跑几轮 O(nb)，远快于 BZ 的 M(nb)logn
        if (nb < BZ_MIN || na - nb + 1 <= KD_QMAX) {
            knuthD(A, na, B, nb, Qout, Rout);
        } else {
            wp = WORK;
            bz_divide(A, na, B, nb, Qout, Rout);
        }
        out = put_big(out, Qout, na - nb + 1);
        *out++ = ' ';
        out = put_big(out, Rout, nb);
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
