// 隔离测试: 验证 fftR4Dif (自包含 radix-4 DIF) 是否为正确的正向 DFT: X[k] = sum_j x[j] exp(-2pi i jk/n)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <complex>
#include <cstdint>
#include <vector>
#include <immintrin.h>
typedef __m128d cpx;
typedef uint32_t u32;
static inline cpx cmul(cpx a, cpx b) {
    return _mm_fmsubadd_pd(_mm_unpacklo_pd(b, b), a, _mm_mul_pd(_mm_unpackhi_pd(b, b), _mm_permute_pd(a, 1)));
}
static inline cpx cadd(cpx a, cpx b) { return _mm_add_pd(a, b); }
static inline cpx csub(cpx a, cpx b) { return _mm_sub_pd(a, b); }
static inline cpx mulI(cpx z) { return _mm_xor_pd(_mm_permute_pd(z, 0x5), _mm_set_pd(0.0, -0.0)); } // i*z  (im 翻转符号)
static inline double re(cpx a) { return ((double*)&a)[1]; }
static inline double im(cpx a) { return ((double*)&a)[0]; }
static inline cpx mk(double r, double i) { return _mm_set_pd(i, r); }

// 复制自 div_4step.cpp (DIF 自然序输入 -> 位反序输出, 末尾位反序置换 -> 自然序)
static void fftR4Dif(cpx* a, u32 n) {
    const int m = 31 - __builtin_clz(n);
    const double twoPi = 6.283185307179586476925286766559;
    u32 len = 4;
    if (m & 1) {
        for (u32 i = 0; i < n; i += 2) {
            cpx x0 = a[i], x1 = a[i + 1];
            a[i] = cadd(x0, x1);
            a[i + 1] = csub(x0, x1);
        }
    }
    for (; len <= n; len <<= 2) {
        const u32 q = len >> 2;
        std::vector<cpx> W(q);
        for (u32 k = 0; k < q; ++k) {
            const double ang = -twoPi * (double)k / (double)len;
            W[k] = _mm_set_pd(std::sin(ang), std::cos(ang));
        }
        for (u32 i = 0; i < n; i += len) {
            for (u32 k = 0; k < q; ++k) {
                cpx x0 = a[i + k], x1 = a[i + k + q], x2 = a[i + k + 2 * q], x3 = a[i + k + 3 * q];
                cpx P0 = cadd(x0, x2), P2 = csub(x0, x2);
                cpx Q0 = cadd(x1, x3), Q2 = csub(x1, x3);
                cpx iQ2 = mulI(Q2);
                cpx A0 = cadd(P0, Q0);
                cpx A2 = cmul(csub(P0, Q0), W[2 * k]);
                cpx A1 = cmul(csub(P2, iQ2), W[k]);
                cpx A3 = cmul(cadd(P2, iQ2), W[3 * k]);
                a[i + k] = A0;
                a[i + k + q] = A1;
                a[i + k + 2 * q] = A2;
                a[i + k + 3 * q] = A3;
            }
        }
    }
    // 末尾位反序置换 -> 自然序输出 (与 fftSmall 同序)
    for (u32 i = 1, j = 0; i < n; ++i) {
        u32 bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { cpx t = a[i]; a[i] = a[j]; a[j] = t; }
    }
}

int main() {
    const double twoPi = 6.283185307179586476925286766559;
    int worst = 0;
    u32 ns[] = {4, 8, 16, 32, 64, 128, 256, 512};
    for (u32 ni = 0; ni < 6; ++ni) {
        const u32 n = ns[ni];
        cpx* x = new cpx[n];
        double* ref = new double[n * 2];
        srand(12345 + n);
        for (u32 j = 0; j < n; ++j) {
            double r = (double)rand()/RAND_MAX*2-1, i=(double)rand()/RAND_MAX*2-1;
            x[j] = mk(r, i);
        }
        for (u32 k = 0; k < n; ++k) {
            double sr=0, si=0;
            for (u32 j = 0; j < n; ++j) {
                double ang = -twoPi*(double)j*(double)k/n;
                sr += re(x[j])*cos(ang) - im(x[j])*sin(ang);
                si += re(x[j])*sin(ang) + im(x[j])*cos(ang);
            }
            ref[2*k]=sr; ref[2*k+1]=si;
        }
        fftR4Dif(x, n);
        double maxerr=0;
        for (u32 p = 0; p < n; ++p) {
            double e = fabs(re(x[p])-ref[2*p]) + fabs(im(x[p])-ref[2*p+1]);
            if (e>maxerr) maxerr=e;
        }
        printf("n=%u  maxerr=%.3e  %s\n", n, maxerr, maxerr<1e-9?"PASS":"FAIL");
        if (maxerr>=1e-9) worst=1;
        delete[] x; delete[] ref;
    }
    return worst;
}
