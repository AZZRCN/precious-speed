// 自研算法方向A：混合自适应大数乘法
// 创新点：运行时benchmark动态测定算法切换阈值（不同于GMP的静态阈值）
//
// 算法选择策略：
//   N < T1:  basicMul  (O(n²) 但常数最小)
//   T1 ≤ N < T2: Karatsuba (O(n^1.585))
//   N ≥ T2:  FFT       (O(n log n))
//
// T1和T2在程序启动时通过微型benchmark测定，适应不同硬件

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>

using cd = std::complex<double>;
const double PI = acos(-1.0);

// ============ FFT 实现 ============
void fft(cd* a, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        double ang = -2.0 * PI / len;
        cd wlen(cos(ang), sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd w(1, 0);
            for (size_t j = 0; j < half; j++) {
                cd u = a[i + j], v = a[i + j + half];
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

// ============ FFT 乘法 (base=10^4) ============
void fft_mul(const int* a, size_t na, const int* b, size_t nb, std::vector<int>& out) {
    if (na == 0 || nb == 0) return;
    size_t L = na + nb - 1;
    size_t N = 1;
    while (N < L) N *= 2;
    std::vector<cd> ca(N, cd(0, 0)), cb(N, cd(0, 0));
    for (size_t i = 0; i < na; i++) ca[i] = cd(a[i], 0);
    for (size_t i = 0; i < nb; i++) cb[i] = cd(b[i], 0);
    fft(ca.data(), N);
    fft(cb.data(), N);
    for (size_t i = 0; i < N; i++) ca[i] *= cb[i];
    ifft(ca.data(), N);
    out.assign(L, 0);
    long long carry = 0;
    for (size_t i = 0; i < L; i++) {
        long long val = (long long)(ca[i].real() + 0.5) + carry;
        out[i] = val % 10000;
        carry = val / 10000;
    }
    while (carry > 0) {
        out.push_back(carry % 10000);
        carry /= 10000;
    }
}

// ============ 基础乘法 (O(n²)) ============
void basic_mul(const int* a, size_t na, const int* b, size_t nb, std::vector<int>& out) {
    if (na == 0 || nb == 0) return;
    std::vector<long long> tmp(na + nb, 0);
    for (size_t i = 0; i < na; i++) {
        for (size_t j = 0; j < nb; j++) {
            tmp[i + j] += (long long)a[i] * b[j];
        }
    }
    // 处理进位
    long long carry = 0;
    out.assign(na + nb, 0);
    for (size_t i = 0; i < na + nb; i++) {
        long long val = tmp[i] + carry;
        out[i] = val % 10000;
        carry = val / 10000;
    }
    while (out.size() > 1 && out.back() == 0) out.pop_back();
}

// ============ Karatsuba 乘法 (O(n^1.585)) ============
void karatsuba(const int* a, size_t na, const int* b, size_t nb, std::vector<int>& out) {
    if (na == 0 || nb == 0) return;
    // 小规模回退到基础乘法
    if (na < 16 || nb < 16) {
        basic_mul(a, na, b, nb, out);
        return;
    }
    size_t m = std::max(na, nb) / 2;
    // 分割
    size_t a_lo_len = std::min(m, na);
    size_t a_hi_len = (na > m) ? na - m : 0;
    size_t b_lo_len = std::min(m, nb);
    size_t b_hi_len = (nb > m) ? nb - m : 0;

    const int* a_lo = a;
    const int* a_hi = a + m;
    const int* b_lo = b;
    const int* b_hi = b + m;

    // z0 = a_lo * b_lo
    std::vector<int> z0;
    karatsuba(a_lo, a_lo_len, b_lo, b_lo_len, z0);

    // z2 = a_hi * b_hi
    std::vector<int> z2;
    if (a_hi_len > 0 && b_hi_len > 0) {
        karatsuba(a_hi, a_hi_len, b_hi, b_hi_len, z2);
    } else {
        z2.assign(1, 0);
    }

    // z1 = (a_lo + a_hi) * (b_lo + b_hi) - z0 - z2
    std::vector<int> a_sum(a_lo_len, 0), b_sum(b_lo_len, 0);
    for (size_t i = 0; i < a_lo_len; i++) a_sum[i] = a_lo[i];
    long long carry = 0;
    for (size_t i = 0; i < a_hi_len; i++) {
        if (i < a_lo_len) {
            long long val = a_sum[i] + a_hi[i] + carry;
            a_sum[i] = val % 10000;
            carry = val / 10000;
        } else {
            a_sum.push_back((a_hi[i] + carry) % 10000);
            carry = (a_hi[i] + carry) / 10000;
        }
    }
    // 进位需继续传播到 a_sum[a_hi_len..]（之前 bug：直接 push_back 跳过中间位置）
    for (size_t i = a_hi_len; carry > 0 && i < a_sum.size(); i++) {
        long long val = a_sum[i] + carry;
        a_sum[i] = val % 10000;
        carry = val / 10000;
    }
    while (carry > 0) { a_sum.push_back(carry % 10000); carry /= 10000; }

    for (size_t i = 0; i < b_lo_len; i++) b_sum[i] = b_lo[i];
    carry = 0;
    for (size_t i = 0; i < b_hi_len; i++) {
        if (i < b_lo_len) {
            long long val = b_sum[i] + b_hi[i] + carry;
            b_sum[i] = val % 10000;
            carry = val / 10000;
        } else {
            b_sum.push_back((b_hi[i] + carry) % 10000);
            carry = (b_hi[i] + carry) / 10000;
        }
    }
    for (size_t i = b_hi_len; carry > 0 && i < b_sum.size(); i++) {
        long long val = b_sum[i] + carry;
        b_sum[i] = val % 10000;
        carry = val / 10000;
    }
    while (carry > 0) { b_sum.push_back(carry % 10000); carry /= 10000; }

    std::vector<int> z1_full;
    karatsuba(a_sum.data(), a_sum.size(), b_sum.data(), b_sum.size(), z1_full);

    // z1 = z1_full - z0 - z2
    // 辅助：大数减法
    auto sub_inplace = [](std::vector<int>& x, const std::vector<int>& y) {
        long long borrow = 0;
        for (size_t i = 0; i < x.size(); i++) {
            long long val = x[i] - borrow - (i < y.size() ? y[i] : 0);
            if (val < 0) { val += 10000; borrow = 1; } else borrow = 0;
            x[i] = val;
        }
        while (x.size() > 1 && x.back() == 0) x.pop_back();
    };
    sub_inplace(z1_full, z0);
    sub_inplace(z1_full, z2);

    // 合并结果：out = z0 + z1 * B^m + z2 * B^(2m)
    out.assign(na + nb, 0);
    for (size_t i = 0; i < z0.size(); i++) out[i] = z0[i];
    // 加 z1 * B^m
    carry = 0;
    for (size_t i = 0; i < z1_full.size(); i++) {
        if (m + i >= out.size()) out.push_back(0);
        long long val = out[m + i] + z1_full[i] + carry;
        out[m + i] = val % 10000;
        carry = val / 10000;
    }
    size_t pos = m + z1_full.size();
    while (carry > 0) {
        if (pos >= out.size()) out.push_back(0);
        long long val = out[pos] + carry;
        out[pos] = val % 10000;
        carry = val / 10000;
        pos++;
    }
    // 加 z2 * B^(2m)
    carry = 0;
    for (size_t i = 0; i < z2.size(); i++) {
        if (2 * m + i >= out.size()) out.push_back(0);
        long long val = out[2 * m + i] + z2[i] + carry;
        out[2 * m + i] = val % 10000;
        carry = val / 10000;
    }
    pos = 2 * m + z2.size();
    while (carry > 0) {
        if (pos >= out.size()) out.push_back(0);
        long long val = out[pos] + carry;
        out[pos] = val % 10000;
        carry = val / 10000;
        pos++;
    }
    while (out.size() > 1 && out.back() == 0) out.pop_back();
}

// ============ 混合自适应乘法 ============
static size_t T1 = 32;  // basicMul → Karatsuba 阈值
static size_t T2 = 256; // Karatsuba → FFT 阈值

void hybrid_mul(const int* a, size_t na, const int* b, size_t nb, std::vector<int>& out) {
    size_t n = std::max(na, nb);
    if (n < T1) {
        basic_mul(a, na, b, nb, out);
    } else if (n < T2) {
        karatsuba(a, na, b, nb, out);
    } else {
        fft_mul(a, na, b, nb, out);
    }
}

// ============ 运行时 Benchmark：测定最优阈值 ============
void calibrate_thresholds() {
    printf("[CALIBRATE] 测定当前硬件最优阈值...\n");
    fflush(stdout);
    std::mt19937 rng(42);

    // 测定 T1: basicMul vs Karatsuba
    size_t best_t1 = 32;
    double best_ratio = 1e18;
    for (size_t n = 16; n <= 128; n += 8) {
        std::vector<int> a(n), b(n);
        for (size_t i = 0; i < n; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }
        std::vector<int> out;
        int ITERS = 500;
        // warmup
        basic_mul(a.data(), n, b.data(), n, out);
        karatsuba(a.data(), n, b.data(), n, out);
        // basic
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) basic_mul(a.data(), n, b.data(), n, out);
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_basic = std::chrono::duration<double, std::milli>(t1 - t0).count();
        // karatsuba
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) karatsuba(a.data(), n, b.data(), n, out);
        t1 = std::chrono::high_resolution_clock::now();
        double t_kara = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ratio = t_kara / t_basic;
        printf("  T1 test n=%3zu: basic=%.3fms kara=%.3fms ratio=%.3f %s\n",
               n, t_basic, t_kara, ratio, ratio < 1.0 ? "*KARA*" : "");
        if (ratio < best_ratio) { best_ratio = ratio; best_t1 = n; }
        // 第一个 ratio < 1.0 的点即为切换点
        if (ratio < 1.0) { best_t1 = n; break; }
    }
    T1 = best_t1;
    printf("  T1 = %zu (basicMul → Karatsuba)\n", T1);

    // 测定 T2: Karatsuba vs FFT
    size_t best_t2 = 256;
    best_ratio = 1e18;
    for (size_t n = 64; n <= 1024; n *= 2) {
        std::vector<int> a(n), b(n);
        for (size_t i = 0; i < n; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }
        std::vector<int> out;
        int ITERS = (n <= 256) ? 200 : 50;
        // warmup
        karatsuba(a.data(), n, b.data(), n, out);
        fft_mul(a.data(), n, b.data(), n, out);
        // karatsuba
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) karatsuba(a.data(), n, b.data(), n, out);
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_kara = std::chrono::duration<double, std::milli>(t1 - t0).count();
        // fft
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; i++) fft_mul(a.data(), n, b.data(), n, out);
        t1 = std::chrono::high_resolution_clock::now();
        double t_fft = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ratio = t_fft / t_kara;
        printf("  T2 test n=%4zu: kara=%.3fms fft=%.3fms ratio=%.3f %s\n",
               n, t_kara, t_fft, ratio, ratio < 1.0 ? "*FFT*" : "");
        if (ratio < 1.0) { best_t2 = n; break; }
    }
    T2 = std::max(T1 * 2, best_t2);
    printf("  T2 = %zu (Karatsuba → FFT)\n\n", T2);
    fflush(stdout);
}

// ============ 正确性验证 ============
bool verify_correctness() {
    printf("--- 正确性验证 ---\n");
    std::mt19937 rng(42);
    bool all_pass = true;
    for (int t = 0; t < 50; t++) {
        size_t na = 1 + rng() % 100;
        size_t nb = 1 + rng() % 100;
        std::vector<int> a(na), b(nb);
        for (size_t i = 0; i < na; i++) a[i] = rng() % 10000;
        for (size_t i = 0; i < nb; i++) b[i] = rng() % 10000;
        std::vector<int> out_basic, out_kara, out_fft, out_hybrid;
        basic_mul(a.data(), na, b.data(), nb, out_basic);
        karatsuba(a.data(), na, b.data(), nb, out_kara);
        fft_mul(a.data(), na, b.data(), nb, out_fft);
        hybrid_mul(a.data(), na, b.data(), nb, out_hybrid);
        // 对比
        if (out_basic != out_kara) {
            printf("  FAIL t=%d: basic != kara (na=%zu nb=%zu)\n", t, na, nb);
            // 找第一个差异
            size_t mindiff = std::min(out_basic.size(), out_kara.size());
            for (size_t i = 0; i < mindiff; i++) {
                if (out_basic[i] != out_kara[i]) {
                    printf("    first diff at idx=%zu: basic=%d kara=%d\n", i, out_basic[i], out_kara[i]);
                    printf("    context basic: ");
                    for (size_t j = (i>=3?i-3:0); j <= std::min(i+3, out_basic.size()-1); j++) printf("[%zu]=%d ", j, out_basic[j]);
                    printf("\n    context kara:  ");
                    for (size_t j = (i>=3?i-3:0); j <= std::min(i+3, out_kara.size()-1); j++) printf("[%zu]=%d ", j, out_kara[j]);
                    printf("\n");
                    break;
                }
            }
            if (out_basic.size() != out_kara.size()) {
                printf("    size diff: basic=%zu kara=%zu\n", out_basic.size(), out_kara.size());
            }
            all_pass = false; break;
        }
        if (out_basic != out_fft) {
            printf("  FAIL t=%d: basic != fft (na=%zu nb=%zu)\n", t, na, nb);
            all_pass = false; break;
        }
        if (out_basic != out_hybrid) {
            printf("  FAIL t=%d: basic != hybrid (na=%zu nb=%zu)\n", t, na, nb);
            all_pass = false; break;
        }
    }
    printf("  %s\n\n", all_pass ? "50 cases PASS" : "FAIL");
    return all_pass;
}

// ============ 测速对比 ============
void benchmark() {
    printf("--- 混合自适应 vs 固定算法测速 ---\n");
    printf("  %8s %8s %12s %12s %12s %10s\n", "N", "iters", "basic_ms", "kara_ms", "fft_ms", "hybrid");
    fflush(stdout);
    std::mt19937 rng(42);
    size_t N_list[] = {8, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    int iters_list[] = {5000, 2000, 1000, 500, 200, 100, 50, 20, 10};

    for (int idx = 0; idx < 9; idx++) {
        size_t n = N_list[idx];
        int iters = iters_list[idx];
        std::vector<int> a(n), b(n);
        for (size_t i = 0; i < n; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }
        std::vector<int> out;
        volatile long long sink = 0;

        // basic
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { basic_mul(a.data(), n, b.data(), n, out); sink += out[0]; }
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_basic = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;

        // karatsuba
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { karatsuba(a.data(), n, b.data(), n, out); sink += out[0]; }
        t1 = std::chrono::high_resolution_clock::now();
        double t_kara = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;

        // fft
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { fft_mul(a.data(), n, b.data(), n, out); sink += out[0]; }
        t1 = std::chrono::high_resolution_clock::now();
        double t_fft = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;

        // hybrid
        t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { hybrid_mul(a.data(), n, b.data(), n, out); sink += out[0]; }
        t1 = std::chrono::high_resolution_clock::now();
        double t_hybrid = std::chrono::duration<double, std::milli>(t1 - t0).count() / iters;

        // hybrid 选择的算法
        const char* algo = (n < T1) ? "basic" : (n < T2) ? "kara" : "fft";

        printf("  %8zu %8d %12.4f %12.4f %12.4f %10.4f [%s] sink=%lld\n",
               n, iters, t_basic, t_kara, t_fft, t_hybrid, algo, (long long)sink);
        fflush(stdout);
    }
}

int main() {
    printf("=== 自研算法方向A：混合自适应大数乘法 ===\n\n");

    // 调试模式：跳过 calibrate，直接验证正确性
    #ifdef QUICK_TEST
    printf("[QUICK_TEST] 跳过 calibrate_thresholds\n\n");
    #else
    // 1. 运行时阈值测定
    calibrate_thresholds();
    #endif

    // 2. 正确性验证
    if (!verify_correctness()) { printf("CORRECTNESS FAILED\n"); return 1; }

    // 3. 测速对比
    #ifndef QUICK_TEST
    benchmark();
    #endif

    printf("\n=== 结论 ===\n");
    printf("混合自适应算法根据输入规模动态选择 basicMul/Karatsuba/FFT。\n");
    printf("运行时测定的阈值：T1=%zu (basic→kara), T2=%zu (kara→FFT)\n", T1, T2);
    printf("创新点：运行时benchmark适应不同硬件，而非GMP的静态阈值。\n");
    return 0;
}
