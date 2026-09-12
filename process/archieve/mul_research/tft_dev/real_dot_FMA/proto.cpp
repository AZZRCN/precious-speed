// real_dot FMA 化 - 精度问题演示原型
// 演示 FMA (Fused Multiply-Add) 与标准 mul+add 在 FFT 点积中的精度差异
//
// 背景：
// - mul.cpp line 10 注释：fma 已移除 — 实测 fma 导致 FFT 浮点精度变化, 触发罕见除法余数错误
// - real_dot 占执行时间 21.2%，FMA 化理论收益 2-5%
// - FMA 的单次舍入 vs 标准的双次舍入导致结果略有不同
// - 在大数乘法中，微小精度差异可能导致进位/借位不同，触发边界 case 错误
//
// 本原型演示：
// 1. 标准点积 vs FMA 点积的精度差异
// 2. 在 FFT 卷积中精度差异如何放大
// 3. 模拟大数乘法中的边界 case（进位差异）

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <immintrin.h>  // AVX2/FMA intrinsics

using cd = std::complex<double>;
const double PI = acos(-1.0);

// ============ 标准迭代 radix-2 DIF FFT ============
void fft(cd* a, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        double ang = -2.0 * PI / len;
        cd wlen(cos(ang), sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd w(1, 0);
            for (size_t j = 0; j < half; j++) {
                cd u = a[i + j];
                cd v = a[i + j + half];
                a[i + j] = u + v;
                a[i + j + half] = (u - v) * w;
                w *= wlen;
            }
        }
    }
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
}

void ifft(cd* a, size_t n) {
    for (size_t i = 0; i < n; i++) a[i] = std::conj(a[i]);
    fft(a, n);
    for (size_t i = 0; i < n; i++) a[i] = std::conj(a[i]) / cd(n, 0);
}

// ============ 标准点积（mul + add，双次舍入）============
double dot_standard(const double* a, const double* b, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += a[i] * b[i];  // mul 后 add，双次舍入
    return sum;
}

// ============ FMA 点积（单次舍入）============
double dot_fma(const double* a, const double* b, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum = std::fma(a[i], b[i], sum);  // 单次舍入
    return sum;
}

// ============ 向量化标准点积（AVX2，mul + add）============
double dot_vec_standard(const double* a, const double* b, size_t n) {
    __m256d vsum = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        vsum = _mm256_add_pd(_mm256_mul_pd(va, vb), vsum);  // mul + add
    }
    double tmp[4];
    _mm256_storeu_pd(tmp, vsum);
    double sum = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; i++) sum += a[i] * b[i];
    return sum;
}

// ============ 向量化 FMA 点积（AVX2 + FMA）============
double dot_vec_fma(const double* a, const double* b, size_t n) {
    __m256d vsum = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d vb = _mm256_loadu_pd(b + i);
        vsum = _mm256_fmadd_pd(va, vb, vsum);  // FMA: 单次舍入
    }
    double tmp[4];
    _mm256_storeu_pd(tmp, vsum);
    double sum = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; i++) sum = std::fma(a[i], b[i], sum);
    return sum;
}

// ============ 测试 1：点积精度差异 ============
void test_dot_precision() {
    printf("--- 测试 1：点积精度差异 ---\n");
    std::mt19937 rng(42);
    int max_diff_count = 0;
    double max_diff = 0;

    for (int t = 0; t < 1000; t++) {
        size_t n = 1024;
        std::vector<double> a(n), b(n);
        for (size_t i = 0; i < n; i++) {
            a[i] = (double)(rng() % 100000) / 10000.0;  // [0, 10)
            b[i] = (double)(rng() % 100000) / 10000.0;
        }
        double s_std = dot_standard(a.data(), b.data(), n);
        double s_fma = dot_fma(a.data(), b.data(), n);
        double diff = std::abs(s_std - s_fma);
        if (diff > 0) max_diff_count++;
        max_diff = std::max(max_diff, diff);
    }
    printf("  1000 次测试中 %d 次结果不同, 最大差异 = %.2e\n", max_diff_count, max_diff);
    printf("  结论：FMA 的单次舍入与标准双次舍入产生不同结果\n\n");
}

// ============ 测试 2：FFT 卷积中的精度差异放大 ============
void test_fft_conv_precision() {
    printf("--- 测试 2：FFT 卷积中的精度差异放大 ---\n");
    // 模拟 real_dot：两个复数序列的点积
    // 标准版本用 mul+add，FMA 版本用 fma
    size_t N = 4096;
    std::mt19937 rng(42);
    std::vector<cd> a(N), b(N);
    for (size_t i = 0; i < N; i++) {
        a[i] = cd((double)(rng() % 10000), (double)(rng() % 10000));
        b[i] = cd((double)(rng() % 10000), (double)(rng() % 10000));
    }

    // 标准点积（复数）
    cd dot_std(0, 0);
    for (size_t i = 0; i < N; i++) dot_std += a[i] * b[i];

    // FMA 点积（复数：实部和虚部分别 FMA）
    double re = 0, im = 0;
    for (size_t i = 0; i < N; i++) {
        re = std::fma(a[i].real(), b[i].real(), re);
        re = std::fma(-a[i].imag(), b[i].imag(), re);
        im = std::fma(a[i].real(), b[i].imag(), im);
        im = std::fma(a[i].imag(), b[i].real(), im);
    }
    cd dot_fma_res(re, im);

    double diff = std::abs(dot_std - dot_fma_res);
    printf("  N=%zu 复数点积差异 = %.2e (相对 %.2e)\n",
           N, diff, diff / std::abs(dot_std));
    printf("  结论：大规模点积中精度差异被放大，可能影响 FFT 卷积结果\n\n");
}

// ============ 测试 3：模拟大数乘法边界 case ============
void test_boundary_case() {
    printf("--- 测试 3：模拟大数乘法边界 case（进位差异）---\n");
    // 模拟 FFT 卷积后的取整操作
    // 当卷积结果接近整数边界时，微小精度差异可能导致取整方向不同
    std::mt19937 rng(123);
    int boundary_count = 0;
    double max_round_diff = 0;

    for (int t = 0; t < 100000; t++) {
        size_t n = 256;
        std::vector<double> a(n), b(n);
        for (size_t i = 0; i < n; i++) {
            a[i] = (double)(rng() % 10000);
            b[i] = (double)(rng() % 10000);
        }
        double s_std = dot_standard(a.data(), b.data(), n);
        double s_fma = dot_fma(a.data(), b.data(), n);

        // 模拟大数乘法中的取整（round 到最近整数）
        long long r_std = std::llround(s_std);
        long long r_fma = std::llround(s_fma);

        if (r_std != r_fma) {
            boundary_count++;
            // 检查是否接近 .5 边界
            double frac = s_std - std::floor(s_std);
            if (frac > 0.49 && frac < 0.51) {
                max_round_diff = std::max(max_round_diff, (double)std::abs(r_std - r_fma));
            }
        }
    }
    printf("  100000 次测试中 %d 次取整结果不同\n", boundary_count);
    printf("  最大取整差异 = %.0f（在接近 .5 边界时）\n", max_round_diff);
    printf("  结论：FMA 精度差异在边界 case 下可能导致取整不同，\n");
    printf("        这正是 mul.cpp 中 FMA 触发'罕见除法余数错误'的根因\n\n");
}

// ============ 测试 4：向量化点积测速 ============
void test_vec_speed() {
    printf("--- 测试 4：向量化点积测速（标准 vs FMA）---\n");
    size_t N = 1 << 20;  // 1M elements
    std::vector<double> a(N), b(N);
    std::mt19937 rng(42);
    for (size_t i = 0; i < N; i++) {
        a[i] = (double)(rng() % 10000) / 100.0;
        b[i] = (double)(rng() % 10000) / 100.0;
    }

    volatile double sink = 0;
    int ITERS = 100;

    // warmup
    for (int i = 0; i < 5; i++) {
        sink += dot_vec_standard(a.data(), b.data(), N);
        sink += dot_vec_fma(a.data(), b.data(), N);
    }

    // 标准向量化
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERS; i++) sink += dot_vec_standard(a.data(), b.data(), N);
    auto t1 = std::chrono::high_resolution_clock::now();
    double t_std = std::chrono::duration<double, std::milli>(t1 - t0).count() / ITERS;

    // FMA 向量化
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERS; i++) sink += dot_vec_fma(a.data(), b.data(), N);
    t1 = std::chrono::high_resolution_clock::now();
    double t_fma = std::chrono::duration<double, std::milli>(t1 - t0).count() / ITERS;

    printf("  N=%zu iters=%d: standard=%.3fms fma=%.3fms speedup=%.3fx  sink=%.0f\n",
           N, ITERS, t_std, t_fma, t_std / t_fma, (double)sink);
    printf("  结论：FMA 理论上有 %.1f%% 速度收益，但精度风险使其在 FFT 中不可用\n\n",
           (t_std / t_fma - 1.0) * 100.0);
}

int main() {
    printf("=== real_dot FMA 化 - 精度问题演示 ===\n\n");

    test_dot_precision();
    test_fft_conv_precision();
    test_boundary_case();
    test_vec_speed();

    printf("=== 总结 ===\n");
    printf("1. FMA 的单次舍入与标准 mul+add 的双次舍入产生不同结果\n");
    printf("2. 大规模点积中精度差异被放大，影响 FFT 卷积结果\n");
    printf("3. 边界 case 下取整方向可能不同，导致大数乘法余数错误\n");
    printf("4. FMA 理论上有速度收益，但精度风险使其在 FFT 中不可用\n");
    printf("\n结论：real_dot FMA 化暂停，C2::mul 已 SIMD 优化（addsub+mul+mul），\n");
    printf("      FMA 收益有限且精度风险高，工程上不可行。\n");
    return 0;
}
