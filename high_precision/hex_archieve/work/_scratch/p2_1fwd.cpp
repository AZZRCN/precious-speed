// HEX multiplication v1
//  I/O   : read(2) 整读 + AVX2 分词 + 掩码化 SSE hex 解析 + 一次 write(2)
//  small : la,lb<=32 走 128x128->256 bit 快路径 (覆盖 small_00 全部)
//  mid   : comba schoolbook
//  big   : AVX2 复数 FFT (移植自 DEC mul 榜一, radix-2 DIF/DIT + 深度优先递归)
// 注意: 绝不使用 #pragma GCC optimize —— LC 会 CE。只允许 target。
#pragma GCC target("avx2,fma,bmi,bmi2,popcnt,lzcnt")

#include <cstdint>
#include <cstring>
#include <cstdlib>
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
static constexpr int MAXC = 100010;   // 1.6M hex / 16

alignas(64) static char inbuf_[PAD + INCAP + PAD];
alignas(64) static char outbuf[OUTCAP + 256];
static char* const inbuf = inbuf_ + PAD;
static u64 A[MAXC + 8], B[MAXC + 8], Rr[2 * MAXC + 16];

// FFT 实数缓冲: lm 最大 2^20 (u<=200020 limbs, kk=14 -> coeffs<=914378)
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

// ============================ AVX2 FFT ============================
// 移植自 DEC mul 榜一实现: bit-reversed twiddle 表 + radix-2 DIF/DIT +
// 深度优先递归 (子树落入 L2 后不再往返 L3) + 实序列打包点乘。
#define FFT_LEAF_LOG 11

namespace fft {
using cpx = __m128d;
static cpx* tw = nullptr;
static u32 twlen = 0;

static inline cpx cmul(cpx a, cpx b) {
    return _mm_fmaddsub_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline cpx cmulconj(cpx a, cpx b) {  // a * conj(b)
    return _mm_fmsubadd_pd(_mm_unpacklo_pd(b, b), a, _mm_mul_pd(_mm_unpackhi_pd(b, b), _mm_permute_pd(a, 1)));
}
static inline cpx cmulspec(cpx a, cpx b) {
    return _mm_fmadd_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline cpx cscale(cpx a, double s) { return _mm_mul_pd(a, _mm_set1_pd(s)); }

static void resize(u32 n) {  // n = 复数点数
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
    cpx* e = s + bs;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, y));
    }
    if (p != e) {
        cpx x = *p, y = p[bs];
        *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y);
    }
}
static inline void bfFwd(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wsw = _mm256_permute_pd(w256, 0x5);
    cpx* e = s + bs;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d ylo = _mm256_unpacklo_pd(y, y), yhi = _mm256_unpackhi_pd(y, y);
        __m256d ym = _mm256_fmaddsub_pd(ylo, w256, _mm256_mul_pd(yhi, wsw));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, ym));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, ym));
    }
    if (p != e) {
        cpx x = *p, y = cmul(p[bs], w);
        *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y);
    }
}
static inline void bfInv(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wlo = _mm256_unpacklo_pd(w256, w256), whi = _mm256_unpackhi_pd(w256, w256);
    cpx* e = s + bs;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d d = _mm256_sub_pd(x, y);
        __m256d ds = _mm256_permute_pd(d, 0x5);
        __m256d o = _mm256_fmsubadd_pd(wlo, d, _mm256_mul_pd(whi, ds));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), o);
    }
    if (p != e) {
        cpx x = *p, y = p[bs];
        *p = _mm_add_pd(x, y), p[bs] = cmulconj(_mm_sub_pd(x, y), w);
    }
}

static void difFlat(cpx* d, u32 n, u32 bb) {
    u32 bc = 1;
    for (u32 bs = n >> 1, st = n; bs; st = bs, bs >>= 1, bc <<= 1) {
        const u32 base = bb * bc;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bfPlain(s, bs); j = 1, s += st; }
        for (; j != bc; ++j, s += st) bfFwd(s, bs, tw[base + j]);
    }
}
static void ditFlat(cpx* d, u32 n, u32 bb) {
    u32 bc = n >> 1;
    for (u32 bs = 1, st = 2; bs != n; bs = st, st <<= 1, bc >>= 1) {
        const u32 base = bb * bc;
        u32 j = 0;
        cpx* s = d;
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
static void pointwiseSq(cpx* F, u32 n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    F[0] = cscale(cmulspec(F[0], F[0]), nf);
    F[1] = cscale(cmul(F[1], F[1]), nf);
    const cpx cjm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), 0));
    const cpx ngm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), (int64_t)(1ull << 63)));
    for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        for (u32 f = bs, b = f + bs - 1; f != be; ++f, --b) {
            cpx Fc = _mm_xor_pd(F[b], cjm);
            cpx fe = _mm_add_pd(F[f], Fc), fo = _mm_sub_pd(F[f], Fc);
            cpx t = (f & 1) ? _mm_xor_pd(tw[f >> 1], ngm) : tw[f >> 1];
            cpx pa = _mm_sub_pd(cmul(fe, fe), cmul(cmul(fo, fo), t));
            cpx eo = cmul(fe, fo);
            cpx pb = _mm_add_pd(eo, eo);
            F[f] = cscale(_mm_add_pd(pa, pb), sf);
            F[b] = _mm_xor_pd(cscale(_mm_sub_pd(pa, pb), sf), cjm);
        }
    }
}
}  // namespace fft

// ============================ bit split / merge ============================
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
    static const size_t gk[11] = {19ull << 4,  18ull << 6,  17ull << 8, 16ull << 10,
                                  15ull << 12, 14ull << 14, 13ull << 16, 12ull << 18,
                                  11ull << 20, 10ull << 22, ~size_t(0)};
    int i = 0;
    while (u > gk[i]) ++i;
    return 19 - i;
}

// ============================ multiply ============================
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
    const bool same = (a == b) && (na == nb);
    std::memset(FB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    if (!same) { std::memset(GB, 0, (size_t)lm * 8); split_b2(b, GB, nb, k); }
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    merge_b2(c, FB, u, k);

}

// ============================ output ============================
static inline char* emit(char* out, const u64* R, int n, int neg) {
    while (n > 1 && R[n - 1] == 0) --n;
    if (n <= 0 || (n == 1 && R[0] == 0)) { *out++ = '0'; *out++ = '\n'; return out; }
    if (neg) *out++ = '-';
    u64 top = R[n - 1];
    int d = 16 - (int)(_lzcnt_u64(top) >> 2);
    hex16_store(top << (64 - 4 * d), out);
    out += d;
    for (int c = n - 2; c >= 0; --c) { hex16_store(R[c], out); out += 16; }
    *out++ = '\n';
    return out;
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
    alignas(64) char tmp[96];

    for (u32 t = 0; t < T; ++t) {
        while (*p <= ' ') ++p;
        int sa = (*p == '-'); p += sa;
        const char* a0 = p;
        int la = tok_len(p); p += la;
        while (*p <= ' ') ++p;
        int sb = (*p == '-'); p += sb;
        const char* b0 = p;
        int lb = tok_len(p); p += lb;
        const int neg = sa ^ sb;

        if (la <= 32 && lb <= 32) {
            // ---------- 快路径: 128x128 -> 256 ----------
            u128 av, bv;
            if (la <= 16) av = hexpart(a0 + la, la);
            else av = ((u128)hexpart(a0 + la - 16, la - 16) << 64) | hexfull(a0 + la);
            if (lb <= 16) bv = hexpart(b0 + lb, lb);
            else bv = ((u128)hexpart(b0 + lb - 16, lb - 16) << 64) | hexfull(b0 + lb);

            const u64 x0 = (u64)av, x1 = (u64)(av >> 64);
            const u64 y0 = (u64)bv, y1 = (u64)(bv >> 64);
            u128 p00 = (u128)x0 * y0;
            u128 p01 = (u128)x0 * y1;
            u128 p10 = (u128)x1 * y0;
            u128 p11 = (u128)x1 * y1;
            u128 mid = (p00 >> 64) + (u64)p01 + (u64)p10;
            u128 hi2 = (mid >> 64) + (p01 >> 64) + (p10 >> 64) + (u64)p11;
            u64 r[4];
            r[0] = (u64)p00;
            r[1] = (u64)mid;
            r[2] = (u64)hi2;
            r[3] = (u64)(hi2 >> 64) + (u64)(p11 >> 64);

            int n = 4;
            while (n > 1 && r[n - 1] == 0) --n;
            if (n == 1 && r[0] == 0) { *out++ = '0'; *out++ = '\n'; continue; }
            *out = '-'; out += neg;
            u64 top = r[n - 1];
            int d = 16 - (int)(_lzcnt_u64(top) >> 2);
            int total = d + ((n - 1) << 4);
            // 右对齐到 tmp[64]，一次 64B 拷贝
            char* w = tmp + 64 - ((size_t)n << 4);
            for (int c = n - 1; c >= 0; --c) { hex16_store(r[c], w); w += 16; }
            const char* src = tmp + 64 - total;
            _mm256_storeu_si256((__m256i*)out, _mm256_loadu_si256((const __m256i*)src));
            _mm256_storeu_si256((__m256i*)(out + 32), _mm256_loadu_si256((const __m256i*)(src + 32)));
            out += total;
            *out++ = '\n';
            continue;
        }

        // ---------- 通用路径 ----------
        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        while (na > 1 && A[na - 1] == 0) --na;
        while (nb > 1 && B[nb - 1] == 0) --nb;
        if ((na == 1 && A[0] == 0) || (nb == 1 && B[0] == 0)) { *out++ = '0'; *out++ = '\n'; continue; }

        const u64 *pa = A, *pb = B;
        int ma = na, mb = nb;
        if (ma < mb) { const u64* t2 = pa; pa = pb; pb = t2; int t3 = ma; ma = mb; mb = t3; }
        if (mb <= 48) mul_bf(pa, ma, pb, mb, Rr);
        else mul_fft(pa, ma, pb, mb, Rr);
        out = emit(out, Rr, ma + mb, neg);
    }

    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    return 0;
}
