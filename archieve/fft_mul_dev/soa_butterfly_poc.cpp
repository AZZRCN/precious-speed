// soa_butterfly_poc.cpp —— 决定性微基准：生产 256 位打包 AoS 蝶形内核 vs SoA 蝶形内核
// 目的：实测 SoA 能否打赢"已 256 位打包 AoS"的生产蝶形 (GLM 冷水#2: 双流 load 惩罚)。
// 公平约定：两边迭代次数 M 相同、每次迭代处理 4 个复数乘、都做真实 store(生产蝶形要 store)、仅内核不同。
//   AoS: 1 次 cmulv = 2 复数乘 -> 迭代内做 2 次 = 4 复数乘; 输入/输出各 2 个 __m256d 加载/存储。
//   SoA: soa_mul4 = 4 复数乘; 输入 4 个 __m256d (a_r,a_i,b_r,b_i), 输出 2 个 (zr,zi)。
// 用法: argv[1] = "aos" | "soa"
#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

static inline __m256d cmulv(__m256d a, __m256d b) {
    return _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a, a), b,
                              _mm256_mul_pd(_mm256_unpackhi_pd(a, a), _mm256_permute_pd(b, 0x5)));
}

// SoA 4 复数乘: zr = ar*br - ai*bi ; zi = ar*bi + ai*br
static inline void soa_mul4(const double* ar, const double* ai,
                            const double* br, const double* bi,
                            double* zr, double* zi) {
    __m256d a_r = _mm256_loadu_pd(ar), a_i = _mm256_loadu_pd(ai);
    __m256d b_r = _mm256_loadu_pd(br), b_i = _mm256_loadu_pd(bi);
    _mm256_storeu_pd(zr, _mm256_fmsub_pd(a_r, b_r, _mm256_mul_pd(a_i, b_i)));
    _mm256_storeu_pd(zi, _mm256_fmadd_pd(a_r, b_i, _mm256_mul_pd(a_i, b_r)));
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "aos";
    const int N = 1 << 16;          // 向量槽位数
    const long M = 8000000L;        // 迭代次数 (每迭代 4 复数乘)
    std::mt19937_64 rng(12345);

    if (strcmp(mode, "aos") == 0) {
        double* in  = (double*)_mm_malloc(N * 4 * sizeof(double), 32); // 2 复数/向量
        double* out = (double*)_mm_malloc(N * 4 * sizeof(double), 32);
        for (int i = 0; i < N * 4; ++i) in[i] = (double)(rng() & 0xfff) / 4096.0 - 0.5;
        __m256d acc = _mm256_setzero_pd();
        for (long it = 0; it < M; ++it) {
            int idx = (int)((it * 7) & (N - 1));
            __m256d a = _mm256_loadu_pd(in + idx * 4);
            __m256d b = _mm256_loadu_pd(in + ((idx + 1) & (N - 1)) * 4);
            __m256d c = cmulv(a, b);
            __m256d d = cmulv(b, a);
            _mm256_storeu_pd(out + idx * 4, c);
            _mm256_storeu_pd(out + ((idx + 1) & (N - 1)) * 4, d);
            acc = _mm256_xor_pd(acc, c);
        }
        double s = 0; for (int i = 0; i < 4; ++i) s += ((double*)&acc)[i];
        printf("AOS kernel done acc=%f\n", s);
        _mm_free(in); _mm_free(out);
    } else {
        double* Fr = (double*)_mm_malloc(N * 4 * sizeof(double), 32);
        double* Fi = (double*)_mm_malloc(N * 4 * sizeof(double), 32);
        double* zr = (double*)_mm_malloc(N * 4 * sizeof(double), 32);
        double* zi = (double*)_mm_malloc(N * 4 * sizeof(double), 32);
        for (int i = 0; i < N * 4; ++i) { Fr[i] = (double)(rng()&0xfff)/4096.0-0.5; Fi[i] = (double)(rng()&0xfff)/4096.0-0.5; }
        __m256d acc = _mm256_setzero_pd();
        for (long it = 0; it < M; ++it) {
            int idx = (int)((it * 7) & (N - 1));
            int jdx = (int)(((idx + 1) & (N - 1)));
            soa_mul4(Fr + idx*4, Fi + idx*4, Fr + jdx*4, Fi + jdx*4, zr + idx*4, zi + idx*4);
            acc = _mm256_xor_pd(acc, _mm256_loadu_pd(zr + idx*4));
        }
        double s = 0; for (int i = 0; i < 4; ++i) s += ((double*)&acc)[i];
        printf("SOA kernel done acc=%f\n", s);
        _mm_free(Fr); _mm_free(Fi); _mm_free(zr); _mm_free(zi);
    }
    return 0;
}
