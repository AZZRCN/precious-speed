#pragma once
// SoA FFT: Fr/Fi 分离布局。向量化蝶形消除 cmulv 洗牌(已证 -7.77%)。
// 仅覆盖纯 2 幂路径 (difRec/ditRec/pointwise/pointwiseSq)，混合 radix 暂不 port。
// 约定: 所有偏移为绝对复数索引; 向量化每轮处理 4 复数(p+=4); 标量尾兜底。
// off = 当前块在 Fr/Fi 中的绝对起点(复数下标); 递归沿 off+k*q 推进子块。
#include <cstdint>
#include <cmath>
#include <immintrin.h>
using u32 = uint32_t; using u64 = uint64_t; using u128 = unsigned __int128;
#ifndef FFT_LEAF_LOG
#define FFT_LEAF_LOG 8
#endif

namespace fft_soa {
alignas(64) static double twlo_r[1u << 12], twlo_i[1u << 12];
alignas(64) static double twhi_r[1u << 12], twhi_i[1u << 12];
static u32 twHalfLog = 0, twHalfSize = 0, twHalfMask = 0, twN = 0;

static void resize(u32 n) {
    if (n == twN) return;
    twN = n;
    const u32 halfLog = (u32)(31 - __builtin_clz(n)) >> 1, halfSize = 1u << halfLog;
    twHalfLog = halfLog; twHalfSize = halfSize; twHalfMask = halfSize - 1;
    const double a0 = std::acos(-1.0) / halfSize, a1 = a0 / halfSize;
    for (u32 i = 0, j = (halfSize * 3) >> 1, p = 0; i != halfSize; p -= halfSize - (j >> __builtin_ctz(++i))) {
        int32_t sp = (int32_t)p;
        double lr = std::cos(sp * a0), li = std::sin(sp * a0);
        twlo_r[i] = lr; twlo_i[i] = li;
        double hr = std::cos(sp * a1), hi = std::sin(sp * a1);
        twhi_r[i] = hr; twhi_i[i] = hi;
    }
}
// twg(i) = lo(i&mask) * hi(i>>halfLog)  [两张分离表: twhi 按 log-index 直接存]
static inline void twg(u32 i, double& tr, double& ti) {
    u32 lo = i & twHalfMask, hi = i >> twHalfLog;
    double lr = twlo_r[lo], li = twlo_i[lo], hr = twhi_r[hi], hi2 = twhi_i[hi];
    tr = lr * hr - li * hi2; ti = lr * hi2 + li * hr;
}
static inline void twlo(u32 i, double hr, double hi2, double& tr, double& ti) {
    u32 lo = i & twHalfMask;
    double lr = twlo_r[lo], li = twlo_i[lo];
    tr = lr * hr - li * hi2; ti = lr * hi2 + li * hr;
}
static inline void twhi(u32 i, double& hr, double& hi2) {
    u32 hi = i >> twHalfLog;
    hr = twhi_r[hi]; hi2 = twhi_i[hi];
}
// 逐位复刻 fft_ref::mulI: xor(shuffle_pd(z,z,1), set_pd(0.0,-0.0)) ; z=(wr,wi)
static inline void mulI(double wr, double wi, double& rr, double& ri) {
    __m128d z = _mm_set_pd(wi, wr);
    __m128d z2 = _mm_shuffle_pd(z, z, 1);
    __m128d z3 = _mm_xor_pd(z2, _mm_set_pd(0.0, -0.0));
    rr = _mm_cvtsd_f64(z3);
    ri = _mm_cvtsd_f64(_mm_unpackhi_pd(z3, z3));
}

// (ar,ai)*w(wr,wi) ; 向量化: 4 复数/256 (SoA 每复数 1 double)
static inline void cmulv(__m256d A, __m256d B, double wr, double wi, __m256d& rr, __m256d& ri) {
    __m256d Wr = _mm256_set1_pd(wr), Wi = _mm256_set1_pd(wi);
    rr = _mm256_fmsub_pd(Wr, A, _mm256_mul_pd(Wi, B));
    ri = _mm256_fmadd_pd(Wr, B, _mm256_mul_pd(Wi, A));
}
static inline void cmulconjv(__m256d A, __m256d B, double wr, double wi, __m256d& rr, __m256d& ri) {
    __m256d Wr = _mm256_set1_pd(wr), Wi = _mm256_set1_pd(wi);
    rr = _mm256_fmadd_pd(Wr, A, _mm256_mul_pd(Wi, B));   // wr*ar + wi*ai
    ri = _mm256_fmsub_pd(Wr, B, _mm256_mul_pd(Wi, A));   // wr*ai - wi*ar
}

static void bfPlain(double* Fr, double* Fi, u32 s, u32 bs) {
    u32 e = s + bs, p = s;
    for (; p + 4 <= e; p += 4) {
        __m256d xr = _mm256_loadu_pd(Fr + p), xi = _mm256_loadu_pd(Fi + p);
        __m256d yr = _mm256_loadu_pd(Fr + p + bs), yi = _mm256_loadu_pd(Fi + p + bs);
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(xr, yr));
        _mm256_storeu_pd(Fi + p, _mm256_add_pd(xi, yi));
        _mm256_storeu_pd(Fr + p + bs, _mm256_sub_pd(xr, yr));
        _mm256_storeu_pd(Fi + p + bs, _mm256_sub_pd(xi, yi));
    }
    while (p != e) {
        double x0r = Fr[p], x0i = Fi[p], y0r = Fr[p + bs], y0i = Fi[p + bs];
        Fr[p] = x0r + y0r; Fi[p] = x0i + y0i;
        Fr[p + bs] = x0r - y0r; Fi[p + bs] = x0i - y0i;
        ++p;
    }
}
static void bfFwd(double* Fr, double* Fi, u32 s, u32 bs, double wr, double wi) {
    double x0r = Fr[s], x0i = Fi[s], y0r = Fr[s + bs], y0i = Fi[s + bs];
    double tr = y0r * wr - y0i * wi, ti = y0r * wi + y0i * wr;   // y0 * w
    Fr[s] = x0r + tr; Fi[s] = x0i + ti;
    Fr[s + bs] = x0r - tr; Fi[s + bs] = x0i - ti;
}
// DIT 尾层 radix-2: conj(x-y)*w 语义 (对齐 fft_ref::bfInv)
static void bfInv(double* Fr, double* Fi, u32 s, u32 bs, double wr, double wi) {
    u32 e = s + bs, p = s;
    for (; p + 4 <= e; p += 4) {
        __m256d xr = _mm256_loadu_pd(Fr + p), xi = _mm256_loadu_pd(Fi + p);
        __m256d yr = _mm256_loadu_pd(Fr + p + bs), yi = _mm256_loadu_pd(Fi + p + bs);
        __m256d dr = _mm256_sub_pd(xr, yr), di = _mm256_sub_pd(xi, yi);
        __m256d pr, pi; cmulconjv(dr, di, wr, wi, pr, pi);   // (x-y)*conj(w)
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(xr, yr));
        _mm256_storeu_pd(Fi + p, _mm256_add_pd(xi, yi));
        _mm256_storeu_pd(Fr + p + bs, pr);
        _mm256_storeu_pd(Fi + p + bs, pi);
    }
    while (p != e) {
        double xr = Fr[p], xi = Fi[p], yr = Fr[p + bs], yi = Fi[p + bs];
        double dr = xr - yr, di = xi - yi;
        double pr = dr * wr + di * wi, pi = di * wr - dr * wi;  // (x-y)*conj(w)
        Fr[p] = xr + yr; Fi[p] = xi + yi;
        Fr[p + bs] = pr; Fi[p + bs] = pi;
        ++p;
    }
}
static void bf2Fwd(double* Fr, double* Fi, u32 s, u32 q, double w0r, double w0i,
                   double w1r, double w1i, double w2r, double w2i) {
    const u32 bs = q << 1;
    u32 e = s + q, p = s;
    for (; p + 4 <= e; p += 4) {
        __m256d x0r = _mm256_loadu_pd(Fr + p), x0i = _mm256_loadu_pd(Fi + p);
        __m256d x1r = _mm256_loadu_pd(Fr + p + q), x1i = _mm256_loadu_pd(Fi + p + q);
        __m256d x2r = _mm256_loadu_pd(Fr + p + bs), x2i = _mm256_loadu_pd(Fi + p + bs);
        __m256d x3r = _mm256_loadu_pd(Fr + p + bs + q), x3i = _mm256_loadu_pd(Fi + p + bs + q);
        __m256d t2r, t2i, t3r, t3i, a0r, a0i, a2r, a2i, a1r, a1i, a3r, a3i, u1r, u1i, u3r, u3i;
        cmulv(x2r, x2i, w0r, w0i, t2r, t2i);
        cmulv(x3r, x3i, w0r, w0i, t3r, t3i);
        a0r = _mm256_add_pd(x0r, t2r); a0i = _mm256_add_pd(x0i, t2i);
        a2r = _mm256_sub_pd(x0r, t2r); a2i = _mm256_sub_pd(x0i, t2i);
        a1r = _mm256_add_pd(x1r, t3r); a1i = _mm256_add_pd(x1i, t3i);
        a3r = _mm256_sub_pd(x1r, t3r); a3i = _mm256_sub_pd(x1i, t3i);
        cmulv(a1r, a1i, w1r, w1i, u1r, u1i);
        cmulv(a3r, a3i, w2r, w2i, u3r, u3i);
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(a0r, u1r)); _mm256_storeu_pd(Fi + p, _mm256_add_pd(a0i, u1i));
        _mm256_storeu_pd(Fr + p + q, _mm256_sub_pd(a0r, u1r)); _mm256_storeu_pd(Fi + p + q, _mm256_sub_pd(a0i, u1i));
        _mm256_storeu_pd(Fr + p + bs, _mm256_add_pd(a2r, u3r)); _mm256_storeu_pd(Fi + p + bs, _mm256_add_pd(a2i, u3i));
        _mm256_storeu_pd(Fr + p + bs + q, _mm256_sub_pd(a2r, u3r)); _mm256_storeu_pd(Fi + p + bs + q, _mm256_sub_pd(a2i, u3i));
    }
    while (p != e) {
        double x0r = Fr[p], x0i = Fi[p], x1r = Fr[p + q], x1i = Fi[p + q];
        double x2r = Fr[p + bs], x2i = Fi[p + bs], x3r = Fr[p + bs + q], x3i = Fi[p + bs + q];
        double t2r = w0r * x2r - w0i * x2i, t2i = w0r * x2i + w0i * x2r;
        double t3r = w0r * x3r - w0i * x3i, t3i = w0r * x3i + w0i * x3r;
        double a0r = x0r + t2r, a0i = x0i + t2i, a2r = x0r - t2r, a2i = x0i - t2i;
        double a1r = x1r + t3r, a1i = x1i + t3i, a3r = x1r - t3r, a3i = x1i - t3i;
        double u1r = w1r * a1r - w1i * a1i, u1i = w1r * a1i + w1i * a1r;
        double u3r = w2r * a3r - w2i * a3i, u3i = w2r * a3i + w2i * a3r;
        Fr[p] = a0r + u1r; Fi[p] = a0i + u1i;
        Fr[p + q] = a0r - u1r; Fi[p + q] = a0i - u1i;
        Fr[p + bs] = a2r + u3r; Fi[p + bs] = a2i + u3i;
        Fr[p + bs + q] = a2r - u3r; Fi[p + bs + q] = a2i - u3i;
        ++p;
    }
}
static void bf2FwdOne(double* Fr, double* Fi, u32 s, u32 q, double w2r, double w2i) {
    const u32 bs = q << 1;
    u32 e = s + q, p = s;
    for (; p + 4 <= e; p += 4) {
        __m256d x0r = _mm256_loadu_pd(Fr + p), x0i = _mm256_loadu_pd(Fi + p);
        __m256d x1r = _mm256_loadu_pd(Fr + p + q), x1i = _mm256_loadu_pd(Fi + p + q);
        __m256d x2r = _mm256_loadu_pd(Fr + p + bs), x2i = _mm256_loadu_pd(Fi + p + bs);
        __m256d x3r = _mm256_loadu_pd(Fr + p + bs + q), x3i = _mm256_loadu_pd(Fi + p + bs + q);
        __m256d a0r = _mm256_add_pd(x0r, x2r), a0i = _mm256_add_pd(x0i, x2i);
        __m256d a2r = _mm256_sub_pd(x0r, x2r), a2i = _mm256_sub_pd(x0i, x2i);
        __m256d a1r = _mm256_add_pd(x1r, x3r), a1i = _mm256_add_pd(x1i, x3i);
        __m256d a3r = _mm256_sub_pd(x1r, x3r), a3i = _mm256_sub_pd(x1i, x3i);
        __m256d u3r, u3i;
        cmulv(a3r, a3i, w2r, w2i, u3r, u3i);
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(a0r, a1r)); _mm256_storeu_pd(Fi + p, _mm256_add_pd(a0i, a1i));
        _mm256_storeu_pd(Fr + p + q, _mm256_sub_pd(a0r, a1r)); _mm256_storeu_pd(Fi + p + q, _mm256_sub_pd(a0i, a1i));
        _mm256_storeu_pd(Fr + p + bs, _mm256_add_pd(a2r, u3r)); _mm256_storeu_pd(Fi + p + bs, _mm256_add_pd(a2i, u3i));
        _mm256_storeu_pd(Fr + p + bs + q, _mm256_sub_pd(a2r, u3r)); _mm256_storeu_pd(Fi + p + bs + q, _mm256_sub_pd(a2i, u3i));
    }
    while (p != e) {
        double x0r = Fr[p], x0i = Fi[p], x1r = Fr[p + q], x1i = Fi[p + q];
        double x2r = Fr[p + bs], x2i = Fi[p + bs], x3r = Fr[p + bs + q], x3i = Fi[p + bs + q];
        double a0r = x0r + x2r, a0i = x0i + x2i, a2r = x0r - x2r, a2i = x0i - x2i;
        double a1r = x1r + x3r, a1i = x1i + x3i, a3r = x1r - x3r, a3i = x1i - x3i;
        double u3r = w2r * a3r - w2i * a3i, u3i = w2r * a3i + w2i * a3r;
        Fr[p] = a0r + a1r; Fi[p] = a0i + a1i;
        Fr[p + q] = a0r - a1r; Fi[p + q] = a0i - a1i;
        Fr[p + bs] = a2r + u3r; Fi[p + bs] = a2i + u3i;
        Fr[p + bs + q] = a2r - u3r; Fi[p + bs + q] = a2i - u3i;
        ++p;
    }
}
static void bf2Inv(double* Fr, double* Fi, u32 s, u32 q, double w0r, double w0i,
                   double w1r, double w1i, double w2r, double w2i) {
    const u32 bs = q << 1;
    u32 e = s + q, p = s;
    for (; p + 4 <= e; p += 4) {
        __m256d x0r = _mm256_loadu_pd(Fr + p), x0i = _mm256_loadu_pd(Fi + p);
        __m256d x1r = _mm256_loadu_pd(Fr + p + q), x1i = _mm256_loadu_pd(Fi + p + q);
        __m256d x2r = _mm256_loadu_pd(Fr + p + bs), x2i = _mm256_loadu_pd(Fi + p + bs);
        __m256d x3r = _mm256_loadu_pd(Fr + p + bs + q), x3i = _mm256_loadu_pd(Fi + p + bs + q);
        __m256d a0r = _mm256_add_pd(x0r, x1r), a0i = _mm256_add_pd(x0i, x1i);   // a0 = x0+x1
        __m256d a2r = _mm256_add_pd(x2r, x3r), a2i = _mm256_add_pd(x2i, x3i);   // a2 = x2+x3
        __m256d dx0r = _mm256_sub_pd(x0r, x1r), dx0i = _mm256_sub_pd(x0i, x1i);
        __m256d dx2r = _mm256_sub_pd(x2r, x3r), dx2i = _mm256_sub_pd(x2i, x3i);
        __m256d a1r, a1i, a3r, a3i, da0r, da0i, da1r, da1i, o0r, o0i, o1r, o1i;
        cmulconjv(dx0r, dx0i, w1r, w1i, a1r, a1i);      // a1 = conj(x0-x1)*w1
        cmulconjv(dx2r, dx2i, w2r, w2i, a3r, a3i);      // a3 = conj(x2-x3)*w2
        da0r = _mm256_sub_pd(a0r, a2r); da0i = _mm256_sub_pd(a0i, a2i);
        da1r = _mm256_sub_pd(a1r, a3r); da1i = _mm256_sub_pd(a1i, a3i);
        cmulconjv(da0r, da0i, w0r, w0i, o0r, o0i);       // conj(a0-a2)*w0
        cmulconjv(da1r, da1i, w0r, w0i, o1r, o1i);       // conj(a1-a3)*w0
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(a0r, a2r)); _mm256_storeu_pd(Fi + p, _mm256_add_pd(a0i, a2i));
        _mm256_storeu_pd(Fr + p + bs, o0r); _mm256_storeu_pd(Fi + p + bs, o0i);
        _mm256_storeu_pd(Fr + p + q, _mm256_add_pd(a1r, a3r)); _mm256_storeu_pd(Fi + p + q, _mm256_add_pd(a1i, a3i));
        _mm256_storeu_pd(Fr + p + bs + q, o1r); _mm256_storeu_pd(Fi + p + bs + q, o1i);
    }
    while (p != e) {
        double x0r = Fr[p], x0i = Fi[p], x1r = Fr[p + q], x1i = Fi[p + q];
        double x2r = Fr[p + bs], x2i = Fi[p + bs], x3r = Fr[p + bs + q], x3i = Fi[p + bs + q];
        double a0r = x0r + x1r, a0i = x0i + x1i, a2r = x2r + x3r, a2i = x2i + x3i;
        double dx0r = x0r - x1r, dx0i = x0i - x1i, dx2r = x2r - x3r, dx2i = x2i - x3i;
        double a1r = dx0r * w1r + dx0i * w1i, a1i = dx0i * w1r - dx0r * w1i;  // conj(dx0)*w1
        double a3r = dx2r * w2r + dx2i * w2i, a3i = dx2i * w2r - dx2r * w2i;  // conj(dx2)*w2
        double da0r = a0r - a2r, da0i = a0i - a2i, da1r = a1r - a3r, da1i = a1i - a3i;
        double o0r = da0r * w0r + da0i * w0i, o0i = da0i * w0r - da0r * w0i;  // conj(da0)*w0
        double o1r = da1r * w0r + da1i * w0i, o1i = da1i * w0r - da1r * w0i;  // conj(da1)*w0
        Fr[p] = a0r + a2r; Fi[p] = a0i + a2i;
        Fr[p + bs] = o0r; Fi[p + bs] = o0i;
        Fr[p + q] = a1r + a3r; Fi[p + q] = a1i + a3i;
        Fr[p + bs + q] = o1r; Fi[p + bs + q] = o1i;
        ++p;
    }
}
static void bf2InvOne(double* Fr, double* Fi, u32 s, u32 q, double w2r, double w2i) {
    const u32 bs = q << 1;
    u32 e = s + q, p = s;
    for (; p + 4 <= e; p += 4) {
        __m256d x0r = _mm256_loadu_pd(Fr + p), x0i = _mm256_loadu_pd(Fi + p);
        __m256d x1r = _mm256_loadu_pd(Fr + p + q), x1i = _mm256_loadu_pd(Fi + p + q);
        __m256d x2r = _mm256_loadu_pd(Fr + p + bs), x2i = _mm256_loadu_pd(Fi + p + bs);
        __m256d x3r = _mm256_loadu_pd(Fr + p + bs + q), x3i = _mm256_loadu_pd(Fi + p + bs + q);
        __m256d a0r = _mm256_add_pd(x0r, x1r), a0i = _mm256_add_pd(x0i, x1i);
        __m256d a2r = _mm256_add_pd(x2r, x3r), a2i = _mm256_add_pd(x2i, x3i);
        __m256d dx2r = _mm256_sub_pd(x2r, x3r), dx2i = _mm256_sub_pd(x2i, x3i);
        __m256d a1r = _mm256_sub_pd(x0r, x1r), a1i = _mm256_sub_pd(x0i, x1i);
        __m256d a3r, a3i;
        cmulconjv(dx2r, dx2i, w2r, w2i, a3r, a3i);
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(a0r, a2r)); _mm256_storeu_pd(Fi + p, _mm256_add_pd(a0i, a2i));
        _mm256_storeu_pd(Fr + p + bs, _mm256_sub_pd(a0r, a2r)); _mm256_storeu_pd(Fi + p + bs, _mm256_sub_pd(a0i, a2i));
        _mm256_storeu_pd(Fr + p + q, _mm256_add_pd(a1r, a3r)); _mm256_storeu_pd(Fi + p + q, _mm256_add_pd(a1i, a3i));
        _mm256_storeu_pd(Fr + p + bs + q, _mm256_sub_pd(a1r, a3r)); _mm256_storeu_pd(Fi + p + bs + q, _mm256_sub_pd(a1i, a3i));
    }
    while (p != e) {
        double x0r = Fr[p], x0i = Fi[p], x1r = Fr[p + q], x1i = Fi[p + q];
        double x2r = Fr[p + bs], x2i = Fi[p + bs], x3r = Fr[p + bs + q], x3i = Fi[p + bs + q];
        double a0r = x0r + x1r, a0i = x0i + x1i, a2r = x2r + x3r, a2i = x2i + x3i;
        double dx2r = x2r - x3r, dx2i = x2i - x3i;
        double a1r = x0r - x1r, a1i = x0i - x1i;
        double a3r = dx2r * w2r + dx2i * w2i, a3i = dx2i * w2r - dx2r * w2i;
        Fr[p] = a0r + a2r; Fi[p] = a0i + a2i;
        Fr[p + bs] = a0r - a2r; Fi[p + bs] = a0i - a2i;
        Fr[p + q] = a1r + a3r; Fi[p + q] = a1i + a3i;
        Fr[p + bs + q] = a1r - a3r; Fi[p + bs + q] = a1i - a3i;
        ++p;
    }
}

static void difFlat(double* Fr, double* Fi, u32 n, u32 bb, u32 off) {
    u32 bc = 1, bs = n >> 1, st = n;
    for (; bs >= 2; bc <<= 2, st = bs >> 1, bs >>= 2) {
        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        u32 j = 0; u32 s = off;
        if (base == 0) { double r, i; twg(1, r, i); bf2FwdOne(Fr, Fi, s, q, r, i); j = 1; s += st; }
        if ((bc << 1) <= twHalfSize) {
            double hiAr, hiAi, hiBr, hiBi; twhi(base, hiAr, hiAi); twhi(base2, hiBr, hiBi);
            for (; j != bc; ++j, s += st) {
                double w1r, w1i, w0r, w0i, w2r, w2i;
                twlo(base2 + 2 * j, hiBr, hiBi, w1r, w1i);
                twlo(base + j, hiAr, hiAi, w0r, w0i);
                mulI(w1r, w1i, w2r, w2i);
                bf2Fwd(Fr, Fi, s, q, w0r, w0i, w1r, w1i, w2r, w2i);
            }
        } else {
            for (; j != bc; ++j, s += st) {
                double w1r, w1i, w0r, w0i, w2r, w2i;
                twg(base2 + 2 * j, w1r, w1i);
                twg(base + j, w0r, w0i);
                mulI(w1r, w1i, w2r, w2i);
                bf2Fwd(Fr, Fi, s, q, w0r, w0i, w1r, w1i, w2r, w2i);
            }
        }
    }
    if (bs == 1) {
        const u32 base = bb * bc; u32 j = 0; u32 s = off;
        if (base == 0) { bfPlain(Fr, Fi, s, 1); j = 1; s += 2; }
        if (bc <= twHalfSize) {
            double hiAr, hiAi; twhi(base, hiAr, hiAi);
            for (; j != bc; ++j, s += 2) { double wr, wi; twlo(base + j, hiAr, hiAi, wr, wi); bfFwd(Fr, Fi, s, 1, wr, wi); }
        } else {
            for (; j != bc; ++j, s += 2) { double wr, wi; twg(base + j, wr, wi); bfFwd(Fr, Fi, s, 1, wr, wi); }
        }
    }
}
static void ditFlat(double* Fr, double* Fi, u32 n, u32 bb, u32 off) {
    u32 q = 1, bcOut = n >> 2;
    for (; (q << 2) <= n; q <<= 2, bcOut >>= 2) {
        const u32 st = q << 2, base = bb * bcOut, base2 = base << 1;
        u32 j = 0; u32 s = off;
        if (base == 0) { double r, i; twg(1, r, i); bf2InvOne(Fr, Fi, s, q, r, i); j = 1; s += st; }
        if ((bcOut << 1) <= twHalfSize) {
            double hiAr, hiAi, hiBr, hiBi; twhi(base, hiAr, hiAi); twhi(base2, hiBr, hiBi);
            for (; j != bcOut; ++j, s += st) {
                double w1r, w1i, w0r, w0i, w2r, w2i;
                twlo(base2 + 2 * j, hiBr, hiBi, w1r, w1i);
                twlo(base + j, hiAr, hiAi, w0r, w0i);
                mulI(w1r, w1i, w2r, w2i);
                bf2Inv(Fr, Fi, s, q, w0r, w0i, w1r, w1i, w2r, w2i);
            }
        } else {
            for (; j != bcOut; ++j, s += st) {
                double w1r, w1i, w0r, w0i, w2r, w2i;
                twg(base2 + 2 * j, w1r, w1i);
                twg(base + j, w0r, w0i);
                mulI(w1r, w1i, w2r, w2i);
                bf2Inv(Fr, Fi, s, q, w0r, w0i, w1r, w1i, w2r, w2i);
            }
        }
    }
    if ((q << 1) <= n) {
        const u32 base = bb; u32 s = off;
        if (base == 0) bfPlain(Fr, Fi, s, q);
        else { double wr, wi; twg(base, wr, wi); bfInv(Fr, Fi, s, q, wr, wi); }
    }
}
static void difRec(double* Fr, double* Fi, u32 n, u32 bb, u32 off = 0) {
    if (n <= (1u << FFT_LEAF_LOG)) { difFlat(Fr, Fi, n, bb, off); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    if (bb == 0) { double r, i; twg(1, r, i); bf2FwdOne(Fr, Fi, off, q, r, i); }
    else { double w1r, w1i, w0r, w0i, w2r, w2i; twg(bb << 1, w1r, w1i); twg(bb, w0r, w0i); mulI(w1r, w1i, w2r, w2i); bf2Fwd(Fr, Fi, off, q, w0r, w0i, w1r, w1i, w2r, w2i); }
    difRec(Fr, Fi, q, b4,     off);
    difRec(Fr, Fi, q, b4 | 1, off + q);
    difRec(Fr, Fi, q, b4 | 2, off + 2 * q);
    difRec(Fr, Fi, q, b4 | 3, off + 3 * q);
}
static void difZeroHiTop(double* Fr, double* Fi, u32 n, u32 off) {
    const u32 q = n >> 2, bs = q << 1; double w2r, w2i; twg(1, w2r, w2i);
    u32 e = off + q, p = off;
    for (; p + 4 <= e; p += 4) {
        __m256d x0r = _mm256_loadu_pd(Fr + p), x0i = _mm256_loadu_pd(Fi + p);
        __m256d x1r = _mm256_loadu_pd(Fr + p + q), x1i = _mm256_loadu_pd(Fi + p + q);
        __m256d u3r, u3i;
        cmulv(x1r, x1i, w2r, w2i, u3r, u3i);
        _mm256_storeu_pd(Fr + p, _mm256_add_pd(x0r, x1r)); _mm256_storeu_pd(Fi + p, _mm256_add_pd(x0i, x1i));
        _mm256_storeu_pd(Fr + p + q, _mm256_sub_pd(x0r, x1r)); _mm256_storeu_pd(Fi + p + q, _mm256_sub_pd(x0i, x1i));
        _mm256_storeu_pd(Fr + p + bs, _mm256_add_pd(x0r, u3r)); _mm256_storeu_pd(Fi + p + bs, _mm256_add_pd(x0i, u3i));
        _mm256_storeu_pd(Fr + p + bs + q, _mm256_sub_pd(x0r, u3r)); _mm256_storeu_pd(Fi + p + bs + q, _mm256_sub_pd(x0i, u3i));
    }
    while (p != e) {
        double x0r = Fr[p], x0i = Fi[p], x1r = Fr[p + q], x1i = Fi[p + q];
        double u3r = w2r * x1r - w2i * x1i, u3i = w2r * x1i + w2i * x1r;
        Fr[p] = x0r + x1r; Fi[p] = x0i + x1i;
        Fr[p + q] = x0r - x1r; Fi[p + q] = x0i - x1i;
        Fr[p + bs] = x0r + u3r; Fi[p + bs] = x0i + u3i;
        Fr[p + bs + q] = x0r - u3r; Fi[p + bs + q] = x0i - u3i;
        ++p;
    }
}
static void difRecZeroHi(double* Fr, double* Fi, u32 n, u32 off = 0) {
    const u32 q = n >> 2;
    difZeroHiTop(Fr, Fi, n, off);
    difRec(Fr, Fi, q, 0, off);
    difRec(Fr, Fi, q, 1, off + q);
    difRec(Fr, Fi, q, 2, off + 2 * q);
    difRec(Fr, Fi, q, 3, off + 3 * q);
}
static void ditRec(double* Fr, double* Fi, u32 n, u32 bb, u32 off = 0) {
    if (n <= (1u << FFT_LEAF_LOG)) { ditFlat(Fr, Fi, n, bb, off); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    ditRec(Fr, Fi, q, b4,     off);
    ditRec(Fr, Fi, q, b4 | 1, off + q);
    ditRec(Fr, Fi, q, b4 | 2, off + 2 * q);
    ditRec(Fr, Fi, q, b4 | 3, off + 3 * q);
    if (bb == 0) { double r, i; twg(1, r, i); bf2InvOne(Fr, Fi, off, q, r, i); }
    else { double w1r, w1i, w0r, w0i, w2r, w2i; twg(bb << 1, w1r, w1i); twg(bb, w0r, w0i); mulI(w1r, w1i, w2r, w2i); bf2Inv(Fr, Fi, off, q, w0r, w0i, w1r, w1i, w2r, w2i); }
}

// ---- 标量 pointwise (先保比特级正确; 后续可向量化) ----
static inline void cmul_(double ar, double ai, double br, double bi, double& rr, double& ri) {
    rr = ar * br - ai * bi; ri = ar * bi + ai * br;
}
static inline void cmulspec_(double ar, double ai, double br, double bi, double& rr, double& ri) {
    rr = ar * br + ai * bi; ri = ar * bi + ai * br;
}
static void pointwise(double* Fr, double* Fi, double* Gr, double* Gi, u32 n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    double a0r, a0i, b0r, b0i;
    cmulspec_(Fr[0], Fi[0], Gr[0], Gi[0], a0r, a0i); Fr[0] = a0r * nf; Fi[0] = a0i * nf;
    cmul_(Fr[1], Fi[1], Gr[1], Gi[1], b0r, b0i); Fr[1] = b0r * nf; Fi[1] = b0i * nf;
    for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        for (u32 f = bs, b = f + bs - 1; f != be; ++f, --b) {
            double Ffr = Fr[f], Ffi = Fi[f];
            double Fbr = Fr[b], Fbi = -Fi[b];            // conj(F[b])
            double fe_r = Ffr + Fbr, fe_i = Ffi + Fbi;
            double fo_r = Ffr - Fbr, fo_i = Ffi - Fbi;
            double Gfr = Gr[f], Gfi = Gi[f];
            double Gbr = Gr[b], Gbi = -Gi[b];            // conj(G[b])
            double ge_r = Gfr + Gbr, ge_i = Gfi + Gbi;
            double go_r = Gfr - Gbr, go_i = Gfi - Gbi;
            double tr, ti; twg(f >> 1, tr, ti);
            if (f & 1) { tr = -tr; ti = -ti; }
            double feg_r, feg_i, gfo_r, gfo_i, fog_r, fog_i, t_fog_r, t_fog_i, fego_r, fego_i;
            cmul_(fe_r, fe_i, ge_r, ge_i, feg_r, feg_i);     // fe*ge
            cmul_(ge_r, ge_i, fo_r, fo_i, gfo_r, gfo_i);     // ge*fo
            cmul_(fo_r, fo_i, go_r, go_i, fog_r, fog_i);     // fo*go
            cmul_(fe_r, fe_i, go_r, go_i, fego_r, fego_i);   // fe*go
            cmul_(fog_r, fog_i, tr, ti, t_fog_r, t_fog_i);   // (fo*go)*t
            double pa_r = feg_r - t_fog_r, pa_i = feg_i - t_fog_i;
            double pb_r = gfo_r + fego_r, pb_i = gfo_i + fego_i;  // ge*fo + fe*go
            Fr[f] = (pa_r + pb_r) * sf; Fi[f] = (pa_i + pb_i) * sf;
            Fr[b] = (pa_r - pb_r) * sf; Fi[b] = -(pa_i - pb_i) * sf;  // xor cjm 翻转虚部
        }
    }
}
static void pointwiseSq(double* Fr, double* Fi, u32 n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    double a0r, a0i, b0r, b0i;
    cmulspec_(Fr[0], Fi[0], Fr[0], Fi[0], a0r, a0i); Fr[0] = a0r * nf; Fi[0] = a0i * nf;
    cmul_(Fr[1], Fi[1], Fr[1], Fi[1], b0r, b0i); Fr[1] = b0r * nf; Fi[1] = b0i * nf;
    for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        for (u32 f = bs, b = f + bs - 1; f != be; ++f, --b) {
            double Ffr = Fr[f], Ffi = Fi[f];
            double Fbr = Fr[b], Fbi = -Fi[b];            // conj(F[b])
            double fe_r = Ffr + Fbr, fe_i = Ffi + Fbi;
            double fo_r = Ffr - Fbr, fo_i = Ffi - Fbi;
            double tr, ti; twg(f >> 1, tr, ti);
            if (f & 1) { tr = -tr; ti = -ti; }
            double fefe_r, fefe_i, fofo_r, fofo_i, fefo_r, fefo_i, t_fofo_r, t_fofo_i;
            cmul_(fe_r, fe_i, fe_r, fe_i, fefe_r, fefe_i);    // fe*fe
            cmul_(fo_r, fo_i, fo_r, fo_i, fofo_r, fofo_i);    // fo*fo
            cmul_(fe_r, fe_i, fo_r, fo_i, fefo_r, fefo_i);    // fe*fo
            cmul_(fofo_r, fofo_i, tr, ti, t_fofo_r, t_fofo_i);// (fo*fo)*t
            double pa_r = fefe_r - t_fofo_r, pa_i = fefe_i - t_fofo_i;
            double pb_r = 2.0 * fefo_r, pb_i = 2.0 * fefo_i;  // ge*fo + fe*go = 2*fe*fo
            Fr[f] = (pa_r + pb_r) * sf; Fi[f] = (pa_i + pb_i) * sf;
            Fr[b] = (pa_r - pb_r) * sf; Fi[b] = -(pa_i - pb_i) * sf;
        }
    }
}
} // namespace fft_soa
