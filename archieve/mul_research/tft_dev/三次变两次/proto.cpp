// 三次变两次优化 - RIRI 打包正确性验证原型
// 验证：两个实数序列打包为复数序列，1次复数FFT同时得到两个实数序列的FFT
// 对比：RIRI打包(1次FFT) vs 分别FFT(2次FFT) 的正确性和速度
//
// RIRI 打包原理：
//   c[k] = a[k] + i*b[k]  (两个实数序列打包为复数序列)
//   C = FFT(c)
//   分离：A[k] = (C[k] + conj(C[N-k])) / 2
//         B[k] = (C[k] - conj(C[N-k])) / (2i)
//   其中 conj(x) 为共轭，A=FFT(a), B=FFT(b)
//
// 在 mul.cpp 中，dif<true> 的 true 参数即 RIRI_IN，表示输入是 RIRI 打包格式
// fftMul 已用此技术实现"三次变两次"：1 DFT + 1 IDFT = 2 FFT（而非 3 FFT）

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

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
    // 位反转
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
}

// ============ RIRI 打包：1次复数FFT得到两个实数序列的FFT ============
// 输入：两个实数序列 a, b（长度 N）
// 输出：A=FFT(a), B=FFT(b)（长度 N）
// 只需 1 次复数 FFT（而非 2 次分别 FFT）
void riri_fft(const double* a, const double* b, size_t N, cd* A, cd* B) {
    std::vector<cd> c(N);
    for (size_t i = 0; i < N; i++) c[i] = cd(a[i], b[i]);  // RIRI 打包
    fft(c.data(), N);
    // 分离 A 和 B
    // A[k] = (C[k] + conj(C[N-k])) / 2
    // B[k] = (C[k] - conj(C[N-k])) / (2i) = (C[k] - conj(C[N-k])) * (0, -0.5)
    for (size_t k = 0; k < N; k++) {
        size_t km = (k == 0) ? 0 : N - k;  // C[N-k] = C[(N-k)%N]
        cd Ck = c[k];
        cd Cnk = c[km];
        cd sum = Ck + std::conj(Cnk);      // 2 * Re(FFT(a))[k]
        cd diff = Ck - std::conj(Cnk);     // 2i * Im(FFT(b))[k]
        A[k] = sum * 0.5;
        // diff = 2i*B[k] = (-2*bi, 2*br), 所以 B[k] = (diff.imag/2, -diff.real/2)
        B[k] = cd(diff.imag() * 0.5, diff.real() * -0.5);
    }
}

// ============ 分别 FFT（baseline）：2 次复数 FFT ============
void separate_fft(const double* a, const double* b, size_t N, cd* A, cd* B) {
    std::vector<cd> ca(N), cb(N);
    for (size_t i = 0; i < N; i++) { ca[i] = cd(a[i], 0); cb[i] = cd(b[i], 0); }
    fft(ca.data(), N);
    fft(cb.data(), N);
    for (size_t i = 0; i < N; i++) { A[i] = ca[i]; B[i] = cb[i]; }
}

// ============ 正确性验证 ============
bool verify_correctness(size_t N) {
    std::mt19937 rng(42 + N);
    std::vector<double> a(N), b(N);
    for (size_t i = 0; i < N; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }

    std::vector<cd> A_riri(N), B_riri(N), A_sep(N), B_sep(N);
    riri_fft(a.data(), b.data(), N, A_riri.data(), B_riri.data());
    separate_fft(a.data(), b.data(), N, A_sep.data(), B_sep.data());

    double err_A = 0, err_B = 0;
    for (size_t i = 0; i < N; i++) {
        err_A = std::max(err_A, std::abs(A_riri[i] - A_sep[i]));
        err_B = std::max(err_B, std::abs(B_riri[i] - B_sep[i]));
    }
    bool ok = err_A < 1e-6 && err_B < 1e-6;
    printf("  N=%6zu: err_A=%.2e err_B=%.2e  %s\n", N, err_A, err_B, ok ? "PASS" : "FAIL");
    return ok;
}

// ============ 测速对比 ============
void benchmark(size_t N, int iters) {
    std::mt19937 rng(42);
    std::vector<double> a(N), b(N);
    for (size_t i = 0; i < N; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }

    std::vector<cd> A(N), B(N);
    volatile double sink = 0;

    // warmup
    for (int i = 0; i < 3; i++) {
        riri_fft(a.data(), b.data(), N, A.data(), B.data());
        separate_fft(a.data(), b.data(), N, A.data(), B.data());
    }

    // RIRI 打包：1 次 FFT
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; i++) {
        riri_fft(a.data(), b.data(), N, A.data(), B.data());
        sink += A[0].real();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double t_riri = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // 分别 FFT：2 次 FFT
    t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; i++) {
        separate_fft(a.data(), b.data(), N, A.data(), B.data());
        sink += A[0].real();
    }
    t1 = std::chrono::high_resolution_clock::now();
    double t_sep = std::chrono::duration<double, std::milli>(t1 - t0).count();

    double speedup = t_sep / t_riri;
    printf("  N=%6zu iters=%6d: riri=%8.3fms sep=%8.3fms speedup=%.3fx  %s  sink=%.0f\n",
           N, iters, t_riri, t_sep, speedup, speedup > 1.0 ? "*RIRI*" : "", (double)sink);
}

int main() {
    printf("=== 三次变两次优化 (RIRI 打包) 正确性验证 ===\n");
    bool all_pass = true;
    for (size_t N : {(size_t)4, (size_t)16, (size_t)64, (size_t)256, (size_t)1024, (size_t)4096}) {
        if (!verify_correctness(N)) all_pass = false;
    }
    if (!all_pass) { printf("CORRECTNESS FAILED\n"); return 1; }
    printf("All correctness tests PASSED\n\n");

    printf("=== RIRI 打包 vs 分别 FFT 测速 ===\n");
    printf("  (RIRI: 1次复数FFT, 分别: 2次复数FFT, 理论 speedup ~2.0x)\n");
    fflush(stdout);
    int iters_list[] = {100000, 50000, 10000, 5000, 1000, 200};
    size_t N_list[] = {16, 64, 256, 1024, 4096, 16384};
    for (int i = 0; i < 6; i++) {
        benchmark(N_list[i], iters_list[i]);
        fflush(stdout);
    }

    printf("\n=== 结论 ===\n");
    printf("RIRI 打包用 1 次复数 FFT 同时得到两个实数序列的 FFT，\n");
    printf("等价于'三次变两次'优化：fftMul 从 3 FFT 减为 2 FFT。\n");
    printf("mul.cpp 中 dif<true> 的 true 参数即 RIRI_IN，已实现此优化。\n");
    return 0;
}
