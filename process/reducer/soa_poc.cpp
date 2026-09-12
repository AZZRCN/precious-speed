// soa_poc.cpp - Proof-of-concept: AoS vs SoA complex-multiply for the FFT butterfly.
// Goal: show that SoA (separate re[]/im[] registers) removes the 3 shuffles in cmulv,
// replacing them with 2 FMA, at IDENTICAL numeric result. Measured with callgrind I refs.
//
// Build on VM: g++ -O3 -march=haswell -std=c++17 -o soa_poc soa_poc.cpp
// Run:        ./soa_poc            (self-check + micro-bench, prints I-refs delta)
#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

// ---- AoS: 256-bit holds 2 complexes [re0,im0,re1,im1] (current v27 layout) ----
static inline __m256d cmulv_aos(__m256d a, __m256d b) {
    return _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a, a), b,
                              _mm256_mul_pd(_mm256_unpackhi_pd(a, a), _mm256_permute_pd(b, 0x5)));
}

// ---- SoA: 256-bit holds 4 reals (re) + 4 imags (im); 2 complexes per (re,im) pair ----
// a*w where w=(wr,wi):  re' = wr*re - wi*im ;  im' = wr*im + wi*re  (no shuffles)
static inline void cmulv_soa(__m256d re_a, __m256d im_a, __m256d wr, __m256d wi,
                             __m256d& re_o, __m256d& im_o) {
    re_o = _mm256_fmsub_pd(wr, re_a, _mm256_mul_pd(wi, im_a));   // wr*re - wi*im
    im_o = _mm256_fmadd_pd(wr, im_a, _mm256_mul_pd(wi, re_a));   // wr*im + wi*re
}

// Convert AoS [re0,im0,re1,im1] -> SoA (re=[re0,re1,_,_], im=[im0,im1,_,_])
static inline void aos_to_soa(__m256d a, __m256d& re, __m256d& im) {
    re = _mm256_unpacklo_pd(a, _mm256_setzero_pd());  // extract even lanes lazily; demo only
    im = _mm256_unpackhi_pd(a, _mm256_setzero_pd());
}

int main(int argc, char** argv) {
    // ---- 1. numeric equivalence (AoS cmulv vs SoA 2-FMA) on random inputs ----
    unsigned seed = 12345;
    int fails = 0;
    for (int i = 0; i < 100000; ++i) {
        double ar[4], ai[4], br[4], bi[4];
        for (int k = 0; k < 4; ++k) {
            ar[k] = (double)(rand_r(&seed) % 1000) / 7.0 - 70.0;
            ai[k] = (double)(rand_r(&seed) % 1000) / 7.0 - 70.0;
            br[k] = (double)(rand_r(&seed) % 1000) / 7.0 - 70.0;
            bi[k] = (double)(rand_r(&seed) % 1000) / 7.0 - 70.0;
        }
        // AoS: pack 2 complexes
        __m256d a = _mm256_set_pd(ai[1], ar[1], ai[0], ar[0]);
        __m256d b = _mm256_set_pd(bi[1], br[1], bi[0], br[0]);
        __m256d ao = cmulv_aos(a, b);
        double oa[4]; _mm256_storeu_pd(oa, ao);
        // SoA: re_a=[ar0,ar1,0,0], im_a=[ai0,ai1,0,0]; w=(br,bi) per complex
        __m256d re_a = _mm256_set_pd(0, 0, ar[1], ar[0]);
        __m256d im_a = _mm256_set_pd(0, 0, ai[1], ai[0]);
        __m256d wr = _mm256_set_pd(0, 0, br[1], br[0]);
        __m256d wi = _mm256_set_pd(0, 0, bi[1], bi[0]);
        __m256d re_o, im_o;
        cmulv_soa(re_a, im_a, wr, wi, re_o, im_o);
        double ore[4], oim[4]; _mm256_storeu_pd(ore, re_o); _mm256_storeu_pd(oim, im_o);
        // compare: AoS oa = [ai0*bi0..., ] -> oa[0]=re0', oa[1]=im0', oa[2]=re1', oa[3]=im1'
        if (std::fabs(oa[0] - ore[0]) > 1e-9 || std::fabs(oa[1] - oim[0]) > 1e-9 ||
            std::fabs(oa[2] - ore[1]) > 1e-9 || std::fabs(oa[3] - oim[1]) > 1e-9) {
            if (fails++ < 5)
                printf("MISMATCH i=%d aos=[%.6f,%.6f,%.6f,%.6f] soa=[%.6f,%.6f,%.6f,%.6f]\n",
                       i, oa[0], oa[1], oa[2], oa[3], ore[0], oim[0], ore[1], oim[1]);
        }
    }
    printf("SoA vs AoS numeric check: %s (%d mismatches / 100000)\n", fails ? "FAIL" : "OK", fails);

    // ---- 2. micro-bench: I refs of 4M complex multiplies, AoS vs SoA ----
    // mode from argv[1]: "aos" -> only AoS loop; "soa" -> only SoA loop; else both.
    bool mode_aos = true, mode_soa = true;
    if (argc > 1) {
        if (std::strcmp(argv[1], "aos") == 0) mode_soa = false;
        else if (std::strcmp(argv[1], "soa") == 0) mode_aos = false;
    }
    if (mode_aos) {
        const int N = 1024;
        double A[N*4], B[N*4];
        for (int k = 0; k < N*4; ++k) { A[k] = (double)(k%97)/3.0 - 16.0; B[k] = (double)(k%53)/5.0 - 9.0; }
        volatile double sink = 0;
        for (int i = 0; i < 4000000; ++i) {
            int j = (i & (N-1)) * 4;
            __m256d a = _mm256_loadu_pd(A + j);
            __m256d b = _mm256_loadu_pd(B + j);
            __m256d o = cmulv_aos(a, b);
            sink += ((double*)&o)[0];
        }
        (void)sink;
    }
    if (mode_soa) {
        const int N = 1024;
        double AR[N*4], AI[N*4], WR[N*4], WI[N*4];
        for (int k = 0; k < N*4; ++k) {
            AR[k] = (double)(k%97)/3.0 - 16.0; AI[k] = (double)(k%71)/4.0 - 11.0;
            WR[k] = (double)(k%53)/5.0 - 9.0;  WI[k] = (double)(k%41)/6.0 - 7.0;
        }
        volatile double sink = 0;
        for (int i = 0; i < 4000000; ++i) {
            int j = (i & (N-1)) * 4;
            __m256d re_a = _mm256_loadu_pd(AR + j), im_a = _mm256_loadu_pd(AI + j);
            __m256d wr = _mm256_loadu_pd(WR + j), wi = _mm256_loadu_pd(WI + j);
            __m256d re_o, im_o; cmulv_soa(re_a, im_a, wr, wi, re_o, im_o);
            sink += ((double*)&re_o)[0] + ((double*)&im_o)[0];
        }
        (void)sink;
    }
    printf("micro-bench done (run under callgrind to read I refs delta)\n");
    return fails ? 1 : 0;
}
