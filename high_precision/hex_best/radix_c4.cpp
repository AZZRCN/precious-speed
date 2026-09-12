// radix_c4.cpp -- AVX2 C4d 层的 split-radix vs radix-8 对照 (复刻生产 difC4 结构)
// 布局: RRRR|IIII, 每 4 个复数一组 (与生产 c4load/c4store 一致)
// 目的: 用 perf instructions:u 判定 radix-8 在 SIMD 主路径上是否真赢
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <immintrin.h>

#define HINT_AI inline __attribute__((always_inline))
static const double HINT_2PI = 6.283185307179586476925286766559;
static const double S8 = 0.70710678118654752440084436210485;

struct C4d
{
    __m256d re, im;
};
HINT_AI C4d c4load(const double *p) { return C4d{_mm256_load_pd(p), _mm256_load_pd(p + 4)}; }
HINT_AI void c4store(double *p, const C4d &c)
{
    _mm256_store_pd(p, c.re);
    _mm256_store_pd(p + 4, c.im);
}
HINT_AI C4d c4mul(const C4d &a, const C4d &b)
{
    const __m256d t = _mm256_mul_pd(a.im, b.im);
    const __m256d re = _mm256_fmsub_pd(a.re, b.re, t);
    const __m256d u = _mm256_mul_pd(a.im, b.re);
    const __m256d im = _mm256_fmadd_pd(a.re, b.im, u);
    return C4d{re, im};
}
HINT_AI void difSplitC4(C4d &c0, C4d &c1, C4d &c2, C4d &c3)
{
    const __m256d ar = c0.re - c2.re, ai = c0.im - c2.im;
    const __m256d br = c1.re - c3.re, bi = c1.im - c3.im;
    c0.re = c0.re + c2.re; c0.im = c0.im + c2.im;
    c1.re = c1.re + c3.re; c1.im = c1.im + c3.im;
    c2.re = ar + bi; c2.im = ai - br;
    c3.re = ar - bi; c3.im = ai + br;
}

// ================= twiddle 表 (per factor, per rank), C4 布局 =================
// 段 (f, rank) 存 W_rank^{f*j}, j = 0 .. M-1, M = rank/4 (SR) 或 rank/8 (R8)
// 为对照公平, 每个表按各自算法需要的 M 生成
static double *aalloc(size_t n)
{
    void *p = nullptr;
    if (posix_memalign(&p, 32, n * sizeof(double)) != 0) { fprintf(stderr, "oom\n"); exit(1); }
    return (double *)p;
}
struct TwSeg
{
    double *p = nullptr;
    size_t m = 0;
};
// tabs[f][lg] -> seg for rank = 1<<lg
static TwSeg g_tab[8][32];
static void buildSeg(int f, int lg, size_t M)
{
    if (g_tab[f][lg].p && g_tab[f][lg].m >= M) return;
    if (g_tab[f][lg].p) free(g_tab[f][lg].p);
    size_t rank = size_t(1) << lg;
    size_t Mg = ((M + 3) / 4) * 4;
    double *p = aalloc(2 * Mg);
    for (size_t g = 0; g * 4 < Mg; ++g)
        for (int k = 0; k < 4; ++k)
        {
            size_t j = g * 4 + k;
            double th = -HINT_2PI * double(f) * double(j) / double(rank);
            p[8 * g + k] = std::cos(th);
            p[8 * g + 4 + k] = std::sin(th);
        }
    g_tab[f][lg] = TwSeg{p, Mg};
}
static int lgOf(size_t n) { int k = 0; while ((size_t(1) << k) < n) ++k; return k; }

// ================= 标量叶子 (与 radix_cmp.cpp 的 sr_dif 同构, 输入 RRII) ==========
static std::vector<double> SWr, SWi;
static void initSW(int n)
{
    SWr.assign(n, 0); SWi.assign(n, 0);
    for (int i = 0; i < n; ++i) { double th = -HINT_2PI * i / n; SWr[i] = std::cos(th); SWi[i] = std::sin(th); }
}
static void sr_dif_s(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    const int q = N / 4;
    const double *wr = SWr.data(), *wi = SWi.data();
    for (int j = 0; j < q; ++j)
    {
        double *c0 = x + 2 * j, *c1 = c0 + 2 * q, *c2 = c1 + 2 * q, *c3 = c2 + 2 * q;
        double ar = c0[0] - c2[0], ai = c0[1] - c2[1];
        double br = c1[0] - c3[0], bi = c1[1] - c3[1];
        c0[0] += c2[0]; c0[1] += c2[1];
        c1[0] += c3[0]; c1[1] += c3[1];
        double t2r = ar + bi, t2i = ai - br;
        double t3r = ar - bi, t3i = ai + br;
        int i1 = j * step, i3 = 3 * i1;
        double w1 = wr[i1], v1 = wi[i1], w3 = wr[i3], v3 = wi[i3];
        c2[0] = t2r * w1 - t2i * v1; c2[1] = t2r * v1 + t2i * w1;
        c3[0] = t3r * w3 - t3i * v3; c3[1] = t3r * v3 + t3i * w3;
    }
    sr_dif_s(x, N / 2, step * 2);
    sr_dif_s(x + N, N / 4, step * 4);
    sr_dif_s(x + 3 * N / 2, N / 4, step * 4);
}
// packC4: RRII|RRII <-> RRRR|IIII, 自逆 (注意生产的 "RRII" = 每 2 复数一组 r,r,i,i)
static void packC4(double *p, size_t n)
{
    for (size_t i = 0; i + 8 <= n; i += 8)
    {
        __m256d a = _mm256_load_pd(p + i), b = _mm256_load_pd(p + i + 4);
        _mm256_store_pd(p + i, _mm256_permute2f128_pd(a, b, 0x20));
        _mm256_store_pd(p + i + 4, _mm256_permute2f128_pd(a, b, 0x31));
    }
}
// C4 (RRRR|IIII) <-> RIRI (标量 sr_dif_s 的格式)
HINT_AI C4d c4loadRIRI(const double *p)
{
    const __m256d a = _mm256_load_pd(p), b = _mm256_load_pd(p + 4);
    return C4d{_mm256_permute4x64_pd(_mm256_unpacklo_pd(a, b), 0xD8),
               _mm256_permute4x64_pd(_mm256_unpackhi_pd(a, b), 0xD8)};
}
HINT_AI void c4storeRIRI(double *p, const C4d &c)
{
    const __m256d lo = _mm256_unpacklo_pd(c.re, c.im);
    const __m256d hi = _mm256_unpackhi_pd(c.re, c.im);
    _mm256_store_pd(p, _mm256_permute2f128_pd(lo, hi, 0x20));
    _mm256_store_pd(p + 4, _mm256_permute2f128_pd(lo, hi, 0x31));
}
static void c4_to_riri(double *p, size_t n)
{
    for (size_t i = 0; i + 8 <= n; i += 8) c4storeRIRI(p + i, c4load(p + i));
}
static void riri_to_c4(double *p, size_t n)
{
    for (size_t i = 0; i + 8 <= n; i += 8) c4store(p + i, c4loadRIRI(p + i));
}

#ifndef LEAF
#define LEAF 32
#endif

// ================= C4 split-radix (复刻生产 difC4, T99 4x 展开) =================
static void sr_dif_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        c4_to_riri(inout, float_len);
        sr_dif_s(inout, (int)(float_len / 2), sstep);
        return;
    }
    const size_t fft_len = float_len / 2;
    const size_t s1 = float_len / 4, s2 = s1 * 2, s3 = s1 * 3;
    const int lg = lgOf(fft_len);
    const double *tp1 = g_tab[1][lg].p;
    const double *tp3 = g_tab[3][lg].p;
    double *it = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 16, full = tb & ~size_t(3), bi = 0;
    for (; bi < full; bi += 4, it += 32, tp1 += 32, tp3 += 32)
    {
        C4d c0 = c4load(it), c1 = c4load(it + s1), c2 = c4load(it + s2), c3 = c4load(it + s3);
        difSplitC4(c0, c1, c2, c3);
        c4store(it, c0); c4store(it + s1, c1);
        c4store(it + s2, c4mul(c2, c4load(tp1)));
        c4store(it + s3, c4mul(c3, c4load(tp3)));
        C4d d0 = c4load(it + 8), d1 = c4load(it + s1 + 8), d2 = c4load(it + s2 + 8), d3 = c4load(it + s3 + 8);
        difSplitC4(d0, d1, d2, d3);
        c4store(it + 8, d0); c4store(it + s1 + 8, d1);
        c4store(it + s2 + 8, c4mul(d2, c4load(tp1 + 8)));
        c4store(it + s3 + 8, c4mul(d3, c4load(tp3 + 8)));
        C4d e0 = c4load(it + 16), e1 = c4load(it + s1 + 16), e2 = c4load(it + s2 + 16), e3 = c4load(it + s3 + 16);
        difSplitC4(e0, e1, e2, e3);
        c4store(it + 16, e0); c4store(it + s1 + 16, e1);
        c4store(it + s2 + 16, c4mul(e2, c4load(tp1 + 16)));
        c4store(it + s3 + 16, c4mul(e3, c4load(tp3 + 16)));
        C4d f0 = c4load(it + 24), f1 = c4load(it + s1 + 24), f2 = c4load(it + s2 + 24), f3 = c4load(it + s3 + 24);
        difSplitC4(f0, f1, f2, f3);
        c4store(it + 24, f0); c4store(it + s1 + 24, f1);
        c4store(it + s2 + 24, c4mul(f2, c4load(tp1 + 24)));
        c4store(it + s3 + 24, c4mul(f3, c4load(tp3 + 24)));
    }
    for (; bi < tb; ++bi, it += 8, tp1 += 8, tp3 += 8)
    {
        C4d c0 = c4load(it), c1 = c4load(it + s1), c2 = c4load(it + s2), c3 = c4load(it + s3);
        difSplitC4(c0, c1, c2, c3);
        c4store(it, c0); c4store(it + s1, c1);
        c4store(it + s2, c4mul(c2, c4load(tp1)));
        c4store(it + s3, c4mul(c3, c4load(tp3)));
    }
    const size_t stride = float_len / 4;
    sr_dif_c4(inout, stride * 2, sstep * 2);
    sr_dif_c4(inout + stride * 2, stride, sstep * 4);
    sr_dif_c4(inout + stride * 3, stride, sstep * 4);
}

// ================= C4 radix-8, bitrev-compatible (rev3 store) =================
// rev3 = {0,4,2,6,1,5,3,7}, 蝶形直接在主循环展开
static void r8_dif_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        c4_to_riri(inout, float_len);
        sr_dif_s(inout, (int)(float_len / 2), sstep);
        return;
    }
    const size_t fft_len = float_len / 2;
    if (fft_len < 32) { /* 无法 8 分 (需 q>=4 complex) */ }
    const size_t q2 = float_len / 8; // doubles, 块间距
    const int lg = lgOf(fft_len);
    const double *w1 = g_tab[1][lg].p, *w2 = g_tab[2][lg].p, *w3 = g_tab[3][lg].p;
    const double *w4 = g_tab[4][lg].p, *w5 = g_tab[5][lg].p, *w6 = g_tab[6][lg].p, *w7 = g_tab[7][lg].p;
    const __m256d s8 = _mm256_set1_pd(S8);
    const __m256d zero = _mm256_setzero_pd();
    double *base = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 32; // 组数 (每组 4 复数)
    for (size_t g = 0; g < tb; ++g, base += 8, w1 += 8, w2 += 8, w3 += 8, w4 += 8, w5 += 8, w6 += 8, w7 += 8)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
        C4d a0 = c4load(L0), a1 = c4load(L1), a2 = c4load(L2), a3 = c4load(L3);
        C4d a4 = c4load(L4), a5 = c4load(L5), a6 = c4load(L6), a7 = c4load(L7);
        __m256d b0r = a0.re + a4.re, b0i = a0.im + a4.im;
        __m256d b1r = a1.re + a5.re, b1i = a1.im + a5.im;
        __m256d b2r = a2.re + a6.re, b2i = a2.im + a6.im;
        __m256d b3r = a3.re + a7.re, b3i = a3.im + a7.im;
        __m256d e0r = a0.re - a4.re, e0i = a0.im - a4.im;
        __m256d e1r = a1.re - a5.re, e1i = a1.im - a5.im;
        __m256d e2r = a2.re - a6.re, e2i = a2.im - a6.im;
        __m256d e3r = a3.re - a7.re, e3i = a3.im - a7.im;
        __m256d c0r = e0r, c0i = e0i;
        __m256d c1r = _mm256_mul_pd(s8, e1r + e1i), c1i = _mm256_mul_pd(s8, e1i - e1r);
        __m256d c2r = e2i, c2i = zero - e2r;
        __m256d c3r = _mm256_mul_pd(s8, e3i - e3r), c3i = _mm256_mul_pd(s8, zero - (e3i + e3r));
        __m256d s0r = b0r + b2r, s0i = b0i + b2i, d0r = b0r - b2r, d0i = b0i - b2i;
        __m256d s1r = b1r + b3r, s1i = b1i + b3i, d1r = b1r - b3r, d1i = b1i - b3i;
        C4d y0{s0r + s1r, s0i + s1i};
        C4d y4{s0r - s1r, s0i - s1i};
        C4d y2{d0r + d1i, d0i - d1r};
        C4d y6{d0r - d1i, d0i + d1r};
        __m256d t0r = c0r + c2r, t0i = c0i + c2i, f0r = c0r - c2r, f0i = c0i - c2i;
        __m256d t1r = c1r + c3r, t1i = c1i + c3i, f1r = c1r - c3r, f1i = c1i - c3i;
        C4d y1{t0r + t1r, t0i + t1i};
        C4d y5{t0r - t1r, t0i - t1i};
        C4d y3{f0r + f1i, f0i - f1r};
        C4d y7{f0r - f1i, f0i + f1r};
        // store y_r -> 子块 rev3(r) = {0,4,2,6,1,5,3,7}
        c4store(L0, y0);
        c4store(L4, c4mul(y1, c4load(w1)));
        c4store(L2, c4mul(y2, c4load(w2)));
        c4store(L6, c4mul(y3, c4load(w3)));
        c4store(L1, c4mul(y4, c4load(w4)));
        c4store(L5, c4mul(y5, c4load(w5)));
        c4store(L3, c4mul(y6, c4load(w6)));
        c4store(L7, c4mul(y7, c4load(w7)));
    }
    const size_t sub = float_len / 8;
    for (int r = 0; r < 8; ++r) r8_dif_c4(inout + r * sub, sub, sstep * 8);
}

// ============ radix-8 交织 twiddle: 每组 g 连续存 w1..w7 各 8 doubles (56) ============
static double *g_tabI[32];
static size_t g_tabI_m[32];
static void buildR8I(int lg, size_t M)
{
    if (g_tabI[lg] && g_tabI_m[lg] >= M) return;
    if (g_tabI[lg]) free(g_tabI[lg]);
    size_t rank = size_t(1) << lg;
    size_t Mg = ((M + 3) / 4) * 4, G = Mg / 4;
    double *p = aalloc(G * 56);
    for (size_t g = 0; g < G; ++g)
        for (int f = 1; f <= 7; ++f)
            for (int k = 0; k < 4; ++k)
            {
                size_t j = g * 4 + k;
                double th = -HINT_2PI * double(f) * double(j) / double(rank);
                p[g * 56 + (f - 1) * 8 + k] = std::cos(th);
                p[g * 56 + (f - 1) * 8 + 4 + k] = std::sin(th);
            }
    g_tabI[lg] = p;
    g_tabI_m[lg] = Mg;
}
static void r8i_dif_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        c4_to_riri(inout, float_len);
        sr_dif_s(inout, (int)(float_len / 2), sstep);
        return;
    }
    const size_t fft_len = float_len / 2;
    const size_t q2 = float_len / 8;
    const int lg = lgOf(fft_len);
    const double *tw = g_tabI[lg];
    const __m256d s8 = _mm256_set1_pd(S8);
    const __m256d zero = _mm256_setzero_pd();
    double *base = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 32;
    for (size_t g = 0; g < tb; ++g, base += 8, tw += 56)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
        C4d a0 = c4load(L0), a1 = c4load(L1), a2 = c4load(L2), a3 = c4load(L3);
        C4d a4 = c4load(L4), a5 = c4load(L5), a6 = c4load(L6), a7 = c4load(L7);
        __m256d b0r = a0.re + a4.re, b0i = a0.im + a4.im;
        __m256d b1r = a1.re + a5.re, b1i = a1.im + a5.im;
        __m256d b2r = a2.re + a6.re, b2i = a2.im + a6.im;
        __m256d b3r = a3.re + a7.re, b3i = a3.im + a7.im;
        __m256d e0r = a0.re - a4.re, e0i = a0.im - a4.im;
        __m256d e1r = a1.re - a5.re, e1i = a1.im - a5.im;
        __m256d e2r = a2.re - a6.re, e2i = a2.im - a6.im;
        __m256d e3r = a3.re - a7.re, e3i = a3.im - a7.im;
        __m256d c0r = e0r, c0i = e0i;
        __m256d c1r = _mm256_mul_pd(s8, e1r + e1i), c1i = _mm256_mul_pd(s8, e1i - e1r);
        __m256d c2r = e2i, c2i = zero - e2r;
        __m256d c3r = _mm256_mul_pd(s8, e3i - e3r), c3i = _mm256_mul_pd(s8, zero - (e3i + e3r));
        __m256d s0r = b0r + b2r, s0i = b0i + b2i, d0r = b0r - b2r, d0i = b0i - b2i;
        __m256d s1r = b1r + b3r, s1i = b1i + b3i, d1r = b1r - b3r, d1i = b1i - b3i;
        C4d y0{s0r + s1r, s0i + s1i};
        C4d y4{s0r - s1r, s0i - s1i};
        C4d y2{d0r + d1i, d0i - d1r};
        C4d y6{d0r - d1i, d0i + d1r};
        __m256d t0r = c0r + c2r, t0i = c0i + c2i, f0r = c0r - c2r, f0i = c0i - c2i;
        __m256d t1r = c1r + c3r, t1i = c1i + c3i, f1r = c1r - c3r, f1i = c1i - c3i;
        C4d y1{t0r + t1r, t0i + t1i};
        C4d y5{t0r - t1r, t0i - t1i};
        C4d y3{f0r + f1i, f0i - f1r};
        C4d y7{f0r - f1i, f0i + f1r};
        c4store(L0, y0);
        c4store(L4, c4mul(y1, c4load(tw)));
        c4store(L2, c4mul(y2, c4load(tw + 8)));
        c4store(L6, c4mul(y3, c4load(tw + 16)));
        c4store(L1, c4mul(y4, c4load(tw + 24)));
        c4store(L5, c4mul(y5, c4load(tw + 32)));
        c4store(L3, c4mul(y6, c4load(tw + 40)));
        c4store(L7, c4mul(y7, c4load(tw + 48)));
    }
    const size_t sub = float_len / 8;
    for (int r = 0; r < 8; ++r) r8i_dif_c4(inout + r * sub, sub, sstep * 8);
}
static void prepR8I(size_t float_len)
{
    if (float_len <= LEAF) return;
    size_t fft_len = float_len / 2;
    buildR8I(lgOf(fft_len), fft_len / 8);
    prepR8I(float_len / 8);
}
static size_t tabIBytes()
{
    size_t s = 0;
    for (int l = 0; l < 32; ++l)
        if (g_tabI[l]) s += (g_tabI_m[l] / 4) * 56 * sizeof(double);
    return s;
}
static void freeTabI()
{
    for (int l = 0; l < 32; ++l)
        if (g_tabI[l]) { free(g_tabI[l]); g_tabI[l] = nullptr; g_tabI_m[l] = 0; }
}

// ============ C4 radix-4 (rev2 store), 用于混合基底座 ============
static void r4_dif_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        c4_to_riri(inout, float_len);
        sr_dif_s(inout, (int)(float_len / 2), sstep);
        return;
    }
    const size_t fft_len = float_len / 2;
    const size_t q2 = float_len / 4;
    const int lg = lgOf(fft_len);
    const double *w1 = g_tab[1][lg].p, *w2 = g_tab[2][lg].p, *w3 = g_tab[3][lg].p;
    double *base = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 16;
    for (size_t g = 0; g < tb; ++g, base += 8, w1 += 8, w2 += 8, w3 += 8)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        C4d a0 = c4load(L0), a1 = c4load(L1), a2 = c4load(L2), a3 = c4load(L3);
        __m256d s0r = a0.re + a2.re, s0i = a0.im + a2.im;
        __m256d d0r = a0.re - a2.re, d0i = a0.im - a2.im;
        __m256d s1r = a1.re + a3.re, s1i = a1.im + a3.im;
        __m256d d1r = a1.re - a3.re, d1i = a1.im - a3.im;
        C4d y0{s0r + s1r, s0i + s1i};
        C4d y1{d0r + d1i, d0i - d1r};
        C4d y2{s0r - s1r, s0i - s1i};
        C4d y3{d0r - d1i, d0i + d1r};
        c4store(L0, y0);
        c4store(L2, c4mul(y1, c4load(w1)));
        c4store(L1, c4mul(y2, c4load(w2)));
        c4store(L3, c4mul(y3, c4load(w3)));
    }
    const size_t sub = float_len / 4;
    for (int r = 0; r < 4; ++r) r4_dif_c4(inout + r * sub, sub, sstep * 4);
}
// ============ 混合基: radix-8 主体 + radix-4 底座 (交织 twiddle) ============
static void r8h_dif_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        c4_to_riri(inout, float_len);
        sr_dif_s(inout, (int)(float_len / 2), sstep);
        return;
    }
    if (float_len < 64) { r4_dif_c4(inout, float_len, sstep); return; }
    const size_t fft_len = float_len / 2;
    const size_t q2 = float_len / 8;
    const int lg = lgOf(fft_len);
    const double *tw = g_tabI[lg];
    const __m256d s8 = _mm256_set1_pd(S8);
    const __m256d zero = _mm256_setzero_pd();
    double *base = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 32;
    for (size_t g = 0; g < tb; ++g, base += 8, tw += 56)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
        C4d a0 = c4load(L0), a1 = c4load(L1), a2 = c4load(L2), a3 = c4load(L3);
        C4d a4 = c4load(L4), a5 = c4load(L5), a6 = c4load(L6), a7 = c4load(L7);
        __m256d b0r = a0.re + a4.re, b0i = a0.im + a4.im;
        __m256d b1r = a1.re + a5.re, b1i = a1.im + a5.im;
        __m256d b2r = a2.re + a6.re, b2i = a2.im + a6.im;
        __m256d b3r = a3.re + a7.re, b3i = a3.im + a7.im;
        __m256d e0r = a0.re - a4.re, e0i = a0.im - a4.im;
        __m256d e1r = a1.re - a5.re, e1i = a1.im - a5.im;
        __m256d e2r = a2.re - a6.re, e2i = a2.im - a6.im;
        __m256d e3r = a3.re - a7.re, e3i = a3.im - a7.im;
        __m256d c0r = e0r, c0i = e0i;
        __m256d c1r = _mm256_mul_pd(s8, e1r + e1i), c1i = _mm256_mul_pd(s8, e1i - e1r);
        __m256d c2r = e2i, c2i = zero - e2r;
        __m256d c3r = _mm256_mul_pd(s8, e3i - e3r), c3i = _mm256_mul_pd(s8, zero - (e3i + e3r));
        __m256d s0r = b0r + b2r, s0i = b0i + b2i, d0r = b0r - b2r, d0i = b0i - b2i;
        __m256d s1r = b1r + b3r, s1i = b1i + b3i, d1r = b1r - b3r, d1i = b1i - b3i;
        C4d y0{s0r + s1r, s0i + s1i};
        C4d y4{s0r - s1r, s0i - s1i};
        C4d y2{d0r + d1i, d0i - d1r};
        C4d y6{d0r - d1i, d0i + d1r};
        __m256d t0r = c0r + c2r, t0i = c0i + c2i, f0r = c0r - c2r, f0i = c0i - c2i;
        __m256d t1r = c1r + c3r, t1i = c1i + c3i, f1r = c1r - c3r, f1i = c1i - c3i;
        C4d y1{t0r + t1r, t0i + t1i};
        C4d y5{t0r - t1r, t0i - t1i};
        C4d y3{f0r + f1i, f0i - f1r};
        C4d y7{f0r - f1i, f0i + f1r};
        c4store(L0, y0);
        c4store(L4, c4mul(y1, c4load(tw)));
        c4store(L2, c4mul(y2, c4load(tw + 8)));
        c4store(L6, c4mul(y3, c4load(tw + 16)));
        c4store(L1, c4mul(y4, c4load(tw + 24)));
        c4store(L5, c4mul(y5, c4load(tw + 32)));
        c4store(L3, c4mul(y6, c4load(tw + 40)));
        c4store(L7, c4mul(y7, c4load(tw + 48)));
    }
    const size_t sub = float_len / 8;
    for (int r = 0; r < 8; ++r) r8h_dif_c4(inout + r * sub, sub, sstep * 8);
}
static void prepR4(size_t float_len)
{
    if (float_len <= LEAF) return;
    size_t fft_len = float_len / 2;
    int lg = lgOf(fft_len);
    for (int f = 1; f <= 3; ++f) buildSeg(f, lg, fft_len / 4);
    prepR4(float_len / 4);
}
static void prepR8H(size_t float_len)
{
    if (float_len <= LEAF) return;
    if (float_len < 64) { prepR4(float_len); return; }
    buildR8I(lgOf(float_len / 2), (float_len / 2) / 8);
    prepR8H(float_len / 8);
}

// ============ IDIT: 标量 split-radix 逆 (RIRI, 生产 iditSplit 语义) ============
static void sr_idit_s(double *x, int N, int step)
{
    if (N <= 1) return;
    if (N == 2)
    {
        double ar = x[0], ai = x[1], br = x[2], bi = x[3];
        x[0] = ar + br; x[1] = ai + bi;
        x[2] = ar - br; x[3] = ai - bi;
        return;
    }
    const int q = N / 4;
    sr_idit_s(x, N / 2, step * 2);
    sr_idit_s(x + N, N / 4, step * 4);
    sr_idit_s(x + 3 * N / 2, N / 4, step * 4);
    const double *wr = SWr.data(), *wi = SWi.data();
    for (int j = 0; j < q; ++j)
    {
        double *c0 = x + 2 * j, *c1 = c0 + 2 * q, *c2 = c1 + 2 * q, *c3 = c2 + 2 * q;
        int i1 = j * step, i3 = 3 * i1;
        double w1 = wr[i1], v1 = wi[i1], w3 = wr[i3], v3 = wi[i3];
        double a2r = c2[0] * w1 + c2[1] * v1, a2i = c2[1] * w1 - c2[0] * v1;
        double a3r = c3[0] * w3 + c3[1] * v3, a3i = c3[1] * w3 - c3[0] * v3;
        double sr = a2r + a3r, si = a2i + a3i;
        double dr = a2r - a3r, di = a2i - a3i;
        double n0r = c0[0] + sr, n0i = c0[1] + si;
        double n2r = c0[0] - sr, n2i = c0[1] - si;
        double n1r = c1[0] - di, n1i = c1[1] + dr;
        double n3r = c1[0] + di, n3i = c1[1] - dr;
        c0[0] = n0r; c0[1] = n0i;
        c1[0] = n1r; c1[1] = n1i;
        c2[0] = n2r; c2[1] = n2i;
        c3[0] = n3r; c3[1] = n3i;
    }
}
HINT_AI C4d c4mulConj(const C4d &a, const C4d &b)
{
    const __m256d t = _mm256_mul_pd(a.im, b.im);
    const __m256d re = _mm256_fmadd_pd(a.re, b.re, t);
    const __m256d u = _mm256_mul_pd(a.re, b.im);
    const __m256d im = _mm256_fmsub_pd(a.im, b.re, u);
    return C4d{re, im};
}
// C4 radix-4 逆 (r4_dif_c4 的共轭转置; 输入位置序 rev2, 输出自然序)
static void r4_idit_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        sr_idit_s(inout, (int)(float_len / 2), sstep);
        riri_to_c4(inout, float_len);
        return;
    }
    const size_t sub = float_len / 4;
    for (int r = 0; r < 4; ++r) r4_idit_c4(inout + r * sub, sub, sstep * 4);
    const size_t fft_len = float_len / 2;
    const size_t q2 = float_len / 4;
    const int lg = lgOf(fft_len);
    const double *w1 = g_tab[1][lg].p, *w2 = g_tab[2][lg].p, *w3 = g_tab[3][lg].p;
    double *base = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 16;
    for (size_t g = 0; g < tb; ++g, base += 8, w1 += 8, w2 += 8, w3 += 8)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        C4d y0 = c4load(L0);
        C4d y1 = c4mulConj(c4load(L2), c4load(w1));
        C4d y2 = c4mulConj(c4load(L1), c4load(w2));
        C4d y3 = c4mulConj(c4load(L3), c4load(w3));
        __m256d s0r = y0.re + y2.re, s0i = y0.im + y2.im;
        __m256d d0r = y0.re - y2.re, d0i = y0.im - y2.im;
        __m256d s1r = y1.re + y3.re, s1i = y1.im + y3.im;
        __m256d d1r = y1.re - y3.re, d1i = y1.im - y3.im;
        c4store(L0, C4d{s0r + s1r, s0i + s1i});
        c4store(L1, C4d{d0r - d1i, d0i + d1r});
        c4store(L2, C4d{s0r - s1r, s0i - s1i});
        c4store(L3, C4d{d0r + d1i, d0i - d1r});
    }
}
// C4 radix-8 混合基逆 (r8h_dif_c4 的共轭转置)
static void r8h_idit_c4(double *inout, size_t float_len, int sstep)
{
    if (float_len <= LEAF)
    {
        sr_idit_s(inout, (int)(float_len / 2), sstep);
        riri_to_c4(inout, float_len);
        return;
    }
    if (float_len < 64) { r4_idit_c4(inout, float_len, sstep); return; }
    const size_t sub = float_len / 8;
    for (int r = 0; r < 8; ++r) r8h_idit_c4(inout + r * sub, sub, sstep * 8);
    const size_t fft_len = float_len / 2;
    const size_t q2 = float_len / 8;
    const int lg = lgOf(fft_len);
    const double *tw = g_tabI[lg];
    const __m256d s8 = _mm256_set1_pd(S8);
    const __m256d zero = _mm256_setzero_pd();
    double *base = (double *)__builtin_assume_aligned(inout, 32);
    size_t tb = fft_len / 32;
    for (size_t g = 0; g < tb; ++g, base += 8, tw += 56)
    {
        double *L0 = base, *L1 = base + q2, *L2 = base + 2 * q2, *L3 = base + 3 * q2;
        double *L4 = base + 4 * q2, *L5 = base + 5 * q2, *L6 = base + 6 * q2, *L7 = base + 7 * q2;
        C4d y0 = c4load(L0);
        C4d y1 = c4mulConj(c4load(L4), c4load(tw));
        C4d y2 = c4mulConj(c4load(L2), c4load(tw + 8));
        C4d y3 = c4mulConj(c4load(L6), c4load(tw + 16));
        C4d y4 = c4mulConj(c4load(L1), c4load(tw + 24));
        C4d y5 = c4mulConj(c4load(L5), c4load(tw + 32));
        C4d y6 = c4mulConj(c4load(L3), c4load(tw + 40));
        C4d y7 = c4mulConj(c4load(L7), c4load(tw + 48));
        // IDFT4 adjoint on (y0,y2,y4,y6) -> b0..b3
        __m256d s0r = y0.re + y4.re, s0i = y0.im + y4.im;
        __m256d d0r = y0.re - y4.re, d0i = y0.im - y4.im;
        __m256d s1r = y2.re + y6.re, s1i = y2.im + y6.im;
        __m256d d1r = y2.re - y6.re, d1i = y2.im - y6.im;
        __m256d b0r = s0r + s1r, b0i = s0i + s1i;
        __m256d b2r = s0r - s1r, b2i = s0i - s1i;
        __m256d b1r = d0r - d1i, b1i = d0i + d1r;
        __m256d b3r = d0r + d1i, b3i = d0i - d1r;
        // on (y1,y3,y5,y7) -> c0..c3
        __m256d t0r = y1.re + y5.re, t0i = y1.im + y5.im;
        __m256d f0r = y1.re - y5.re, f0i = y1.im - y5.im;
        __m256d t1r = y3.re + y7.re, t1i = y3.im + y7.im;
        __m256d f1r = y3.re - y7.re, f1i = y3.im - y7.im;
        __m256d c0r = t0r + t1r, c0i = t0i + t1i;
        __m256d c2r = t0r - t1r, c2i = t0i - t1i;
        __m256d c1r = f0r - f1i, c1i = f0i + f1r;
        __m256d c3r = f0r + f1i, c3i = f0i - f1r;
        // e_m = c_m * conj(W8^m)
        __m256d e0r = c0r, e0i = c0i;
        __m256d e1r = _mm256_mul_pd(s8, c1r - c1i), e1i = _mm256_mul_pd(s8, c1i + c1r);
        __m256d e2r = zero - c2i, e2i = c2r;
        __m256d e3r = _mm256_mul_pd(s8, zero - (c3r + c3i)), e3i = _mm256_mul_pd(s8, c3r - c3i);
        c4store(L0, C4d{b0r + e0r, b0i + e0i});
        c4store(L4, C4d{b0r - e0r, b0i - e0i});
        c4store(L1, C4d{b1r + e1r, b1i + e1i});
        c4store(L5, C4d{b1r - e1r, b1i - e1i});
        c4store(L2, C4d{b2r + e2r, b2i + e2i});
        c4store(L6, C4d{b2r - e2r, b2i - e2i});
        c4store(L3, C4d{b3r + e3r, b3i + e3i});
        c4store(L7, C4d{b3r - e3r, b3i - e3i});
    }
}

// ================= 表准备 =================
static void prepSR(size_t float_len)
{
    if (float_len <= LEAF) return;
    size_t fft_len = float_len / 2;
    int lg = lgOf(fft_len);
    buildSeg(1, lg, fft_len / 4);
    buildSeg(3, lg, fft_len / 4);
    size_t stride = float_len / 4;
    prepSR(stride * 2);
    prepSR(stride);
}
static void prepR8(size_t float_len)
{
    if (float_len <= LEAF) return;
    size_t fft_len = float_len / 2;
    int lg = lgOf(fft_len);
    for (int f = 1; f <= 7; ++f) buildSeg(f, lg, fft_len / 8);
    prepR8(float_len / 8);
}
static size_t tabBytes()
{
    size_t s = 0;
    for (int f = 0; f < 8; ++f)
        for (int l = 0; l < 32; ++l)
            if (g_tab[f][l].p) s += g_tab[f][l].m * 2 * sizeof(double);
    return s;
}
static void freeTabs()
{
    for (int f = 0; f < 8; ++f)
        for (int l = 0; l < 32; ++l)
            if (g_tab[f][l].p) { free(g_tab[f][l].p); g_tab[f][l] = TwSeg{}; }
}

int main(int argc, char **argv)
{
    int mode = (argc > 1) ? atoi(argv[1]) : 0;
    size_t N = (argc > 2) ? (size_t)atol(argv[2]) : (1 << 19); // fft_len (complex)
    int reps = (argc > 3) ? atoi(argv[3]) : 20;
    size_t fl = 2 * N;
    initSW((int)N);

    if (mode == 0)
    {
        // 正确性: C4 split-radix vs C4 radix-8, 输出逐元素比对
        for (size_t n : {size_t(64), size_t(512), size_t(4096), size_t(32768), size_t(262144), size_t(524288)})
        {
            freeTabs(); initSW((int)n);
            size_t L = 2 * n;
            prepSR(L); prepR8(L);
            double *x = aalloc(L), *a = aalloc(L), *b = aalloc(L);
            std::mt19937_64 g(777);
            std::uniform_real_distribution<double> d(-1, 1);
            for (size_t i = 0; i < L; ++i) x[i] = d(g);
            memcpy(a, x, L * sizeof(double)); sr_dif_c4(a, L, 1);
            memcpy(b, x, L * sizeof(double)); r8_dif_c4(b, L, 1);
            double m = 0, sc = 0;
            for (size_t i = 0; i < L; ++i) { m = std::max(m, std::fabs(a[i] - b[i])); sc = std::max(sc, std::fabs(a[i])); }
            printf("fft_len=%7zu  |r8c4 - src4|_max = %.3e (scale %.3e)  %s\n",
                   n, m, sc, (m < 1e-9 * sc + 1e-12) ? "MATCH" : "*** DIFF ***");
            free(x); free(a); free(b);
        }
        return 0;
    }
    if (mode == 2 || mode == 3 || mode == 6 || mode == 12 || mode == 13)
    {
        // 分项诊断: C4 版 vs 标量 sr_dif_s (真值)
        // C4 输入是 C4 布局; 逻辑复数序列 = packC4(输入) 的 RRII 解释
        for (size_t n : {size_t(64), size_t(128), size_t(512), size_t(4096), size_t(32768), size_t(131072), size_t(262144), size_t(524288), size_t(1048576)})
        {
            freeTabs(); initSW((int)n);
            size_t L = 2 * n;
            freeTabI(); prepSR(L); prepR8(L); prepR8I(L); prepR4(L); prepR8H(L); prepR4(L); prepR8H(L);
            double *xc = aalloc(L), *xr = aalloc(L), *a = aalloc(L);
            std::mt19937_64 g(777);
            std::uniform_real_distribution<double> d(-1, 1);
            for (size_t i = 0; i < L; ++i) xc[i] = d(g);
            memcpy(xr, xc, L * sizeof(double));
            c4_to_riri(xr, L); // C4 -> RIRI
            memcpy(a, xc, L * sizeof(double));
            if (mode == 2) sr_dif_c4(a, L, 1); else if (mode == 6) r8i_dif_c4(a, L, 1); else if (mode == 12) r8h_dif_c4(a, L, 1); else if (mode == 13) r4_dif_c4(a, L, 1); else r8_dif_c4(a, L, 1);
            sr_dif_s(xr, (int)n, 1);
            double m = 0, sc = 0;
            for (size_t i = 0; i < L; ++i) { m = std::max(m, std::fabs(a[i] - xr[i])); sc = std::max(sc, std::fabs(xr[i])); }
            printf("[%s] fft_len=%7zu  err=%.3e (scale %.3e)  %s\n",
                   mode == 2 ? "sr_c4" : (mode == 6 ? "r8i_c4" : (mode == 12 ? "r8h_c4" : (mode == 13 ? "r4_c4" : "r8_c4"))), n, m, sc,
                   (m < 1e-9 * sc + 1e-12) ? "OK" : "*** BAD ***");
            free(xc); free(xr); free(a);
        }
        return 0;
    }
    if (mode == 14 || mode == 15)
    {
        for (size_t n : {size_t(64), size_t(128), size_t(512), size_t(4096), size_t(32768), size_t(131072), size_t(262144), size_t(524288), size_t(1048576)})
        {
            freeTabI(); freeTabs(); initSW((int)n);
            size_t L = 2 * n;
            prepSR(L); prepR4(L); prepR8H(L);
            double *x = aalloc(L), *a = aalloc(L);
            std::mt19937_64 g(4242);
            std::uniform_real_distribution<double> d(-1, 1);
            for (size_t i = 0; i < L; ++i) x[i] = d(g);
            memcpy(a, x, L * sizeof(double));
            if (mode == 14) r8h_dif_c4(a, L, 1); else sr_dif_c4(a, L, 1);
            r8h_idit_c4(a, L, 1);
            double m = 0;
            for (size_t i = 0; i < L; ++i) m = std::max(m, std::fabs(a[i] / double(n) - x[i]));
            printf("[%s] fft_len=%7zu  |a/N - x|_max = %.3e  %s\n",
                   mode == 14 ? "r8h->r8h" : "sr ->r8h", n, m, (m < 1e-10) ? "OK" : "*** BAD ***");
            free(x); free(a);
        }
        return 0;
    }
    if (mode == 9)
    {
        prepSR(fl); size_t bs = tabBytes(); freeTabs();
        prepR8(fl); size_t br = tabBytes(); freeTabs();
        prepR8I(fl); size_t bti = tabIBytes(); freeTabI();
        printf("fft_len=%zu  tw_bytes: split-radix=%zu (%.2f MB)  radix-8=%zu (%.2f MB)  ratio=%.3f\n",
               N, bs, bs / 1048576.0, br, br / 1048576.0, double(br) / double(bs));
        printf("  r8-interleaved tw_bytes=%zu (%.2f MB) ratio=%.3f\n", bti, bti / 1048576.0, double(bti) / double(bs));
        return 0;
    }

    if (mode == 1) prepSR(fl); else if (mode == 5) prepR8I(fl); else if (mode == 7) prepR8H(fl); else if (mode == 8) prepR4(fl); else prepR8(fl);
    double *x = aalloc(fl), *w = aalloc(fl);
    std::mt19937_64 g(999);
    std::uniform_real_distribution<double> d(-1, 1);
    for (size_t i = 0; i < fl; ++i) x[i] = d(g);
    double sink = 0;
    for (int r = 0; r < reps; ++r)
    {
        memcpy(w, x, fl * sizeof(double));
        if (mode == 1) sr_dif_c4(w, fl, 1); else if (mode == 5) r8i_dif_c4(w, fl, 1); else if (mode == 7) r8h_dif_c4(w, fl, 1); else if (mode == 8) r4_dif_c4(w, fl, 1); else r8_dif_c4(w, fl, 1);
        sink += w[0] + w[fl - 1];
    }
    printf("mode=%d fft_len=%zu reps=%d sink=%.6e\n", mode, N, reps, sink);
    return 0;
}
