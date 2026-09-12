// soa_poc2.cpp - FIXED workload normalization of soa_poc.cpp.
// Bug in v1: AoS loop did 2 complex mults/iter, SoA did 4 complex mults/iter,
// both ran 4M iterations => SoA did 2x the work. "+3% I refs" was meaningless.
// Here: both modes do the SAME total complex multiplications M = 16M.
//   aos mode: 8M iterations x 2 mults   (2 loads  + cmulv(5 vec ops) + sink)
//   soa mode: 4M iterations x 4 mults   (4 loads  + 2mul+2fma      + sink)
// Build: g++ -O3 -march=haswell -std=c++17 -o soa_poc2 soa_poc2.cpp
// Measure: valgrind --tool=callgrind ./soa_poc2 aos   /   ./soa_poc2 soa
#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static inline __m256d cmulv_aos(__m256d a, __m256d b) {
    return _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a, a), b,
                              _mm256_mul_pd(_mm256_unpackhi_pd(a, a), _mm256_permute_pd(b, 0x5)));
}
static inline void cmulv_soa(__m256d re_a, __m256d im_a, __m256d wr, __m256d wi,
                             __m256d& re_o, __m256d& im_o) {
    re_o = _mm256_fmsub_pd(wr, re_a, _mm256_mul_pd(wi, im_a));
    im_o = _mm256_fmadd_pd(wr, im_a, _mm256_mul_pd(wi, re_a));
}

static const int N = 1024; // working-set doubles per stream

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s aos|soa [mults_millions]\n", argv[0]); return 2; }
    const long long Mm = (argc > 2) ? atoll(argv[2]) : 16; // total complex mults in millions
    const long long M = Mm * 1000000LL;
    if (std::strcmp(argv[1], "aos") == 0) {
        double A[N*4], B[N*4];
        for (int k = 0; k < N*4; ++k) { A[k] = (double)(k%97)/3.0 - 16.0; B[k] = (double)(k%53)/5.0 - 9.0; }
        volatile double sink = 0;
        const long long iters = M / 2;             // 2 complex mults per iteration
        for (long long i = 0; i < iters; ++i) {
            int j = (int)((i * 4) & (N*4 - 1));
            __m256d a = _mm256_loadu_pd(A + j);
            __m256d b = _mm256_loadu_pd(B + j);
            __m256d o = cmulv_aos(a, b);
            sink += ((double*)&o)[0];
        }
        (void)sink;
        printf("aos done: %lld complex mults\n", iters * 2);
    } else if (std::strcmp(argv[1], "soa") == 0) {
        double AR[N*4], AI[N*4], WR[N*4], WI[N*4];
        for (int k = 0; k < N*4; ++k) {
            AR[k] = (double)(k%97)/3.0 - 16.0; AI[k] = (double)(k%71)/4.0 - 11.0;
            WR[k] = (double)(k%53)/5.0 - 9.0;  WI[k] = (double)(k%41)/6.0 - 7.0;
        }
        volatile double sink = 0;
        const long long iters = M / 4;             // 4 complex mults per iteration
        for (long long i = 0; i < iters; ++i) {
            int j = (int)((i * 4) & (N*4 - 1));
            __m256d re_a = _mm256_loadu_pd(AR + j), im_a = _mm256_loadu_pd(AI + j);
            __m256d wr = _mm256_loadu_pd(WR + j), wi = _mm256_loadu_pd(WI + j);
            __m256d re_o, im_o; cmulv_soa(re_a, im_a, wr, wi, re_o, im_o);
            sink += ((double*)&re_o)[0] + ((double*)&im_o)[0];
        }
        (void)sink;
        printf("soa done: %lld complex mults\n", iters * 4);
    } else { printf("bad mode\n"); return 2; }
    return 0;
}
