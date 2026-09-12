// 隔离测试: 验证 fftSmall (自包含 radix-2 DIF) 是否为正确的正向 DFT: X[k] = sum_j x[j] exp(-2pi i jk/n)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <complex>
#include <cstdint>
#include <immintrin.h>
typedef __m128d cpx;
typedef uint32_t u32;
static inline cpx cmul(cpx a, cpx b) {
    return _mm_fmsubadd_pd(_mm_unpacklo_pd(b, b), a, _mm_mul_pd(_mm_unpackhi_pd(b, b), _mm_permute_pd(a, 1)));
}
static inline cpx cadd(cpx a, cpx b) { return _mm_add_pd(a, b); }
static inline cpx csub(cpx a, cpx b) { return _mm_sub_pd(a, b); }
static inline double re(cpx a) { return ((double*)&a)[1]; }
static inline double im(cpx a) { return ((double*)&a)[0]; }
static inline cpx mk(double r, double i) { return _mm_set_pd(i, r); }

static void fftSmall(cpx* a, u32 n) {
    for (u32 i = 1, j = 0; i < n; ++i) {
        u32 bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { cpx t = a[i]; a[i] = a[j]; a[j] = t; }
    }
    const double twoPi = 6.283185307179586476925286766559;
    for (u32 len = 2; len <= n; len <<= 1) {
        const u32 half = len >> 1;
        const double wstep = -twoPi / (double)len;
        for (u32 i = 0; i < n; i += len) {
            for (u32 k = 0; k < half; ++k) {
                const double ang = wstep * (double)k;
                cpx w = _mm_set_pd(std::sin(ang), std::cos(ang));
                cpx t = cmul(a[i + half + k], w);
                a[i + half + k] = csub(a[i + k], t);
                a[i + k] = cadd(a[i + k], t);
            }
        }
    }
}

int main() {
    const u32 n = 16;
    cpx x[16];
    double ref[16][2];
    // 随机输入
    srand(12345);
    for (u32 j = 0; j < n; ++j) {
        double r = (double)rand()/RAND_MAX*2-1, i=(double)rand()/RAND_MAX*2-1;
        x[j] = mk(r, i);
    }
    // 朴素 DFT (正向, exp(-2pi i jk/n))
    const double twoPi = 6.283185307179586476925286766559;
    for (u32 k = 0; k < n; ++k) {
        double sr=0, si=0;
        for (u32 j = 0; j < n; ++j) {
            double ang = -twoPi*(double)j*(double)k/n;
            sr += re(x[j])*cos(ang) - im(x[j])*sin(ang);
            si += re(x[j])*sin(ang) + im(x[j])*cos(ang);
        }
        ref[k][0]=sr; ref[k][1]=si;
    }
    // fftSmall
    fftSmall(x, n);
    // fftSmall = bitrev 输入 + 蝶形 -> 自然序输出. 直接按自然序比较.
    double maxerr=0;
    for (u32 p = 0; p < n; ++p) {
        double e = fabs(re(x[p])-ref[p][0]) + fabs(im(x[p])-ref[p][1]);
        if (e>maxerr) maxerr=e;
    }
    printf("fftSmall vs naiveDFT maxerr = %.3e  (%s)\n", maxerr, maxerr<1e-9?"PASS":"FAIL");
    return maxerr<1e-9?0:1;
}
