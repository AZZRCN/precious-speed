// 自研算法方向D：分块SIMD矩阵乘法
// 核心验证：大数乘法能否转化为矩阵乘法并获得性能优势
//
// 理论分析：
// - 大数乘法 = 多项式乘法 = 卷积
// - 卷积可表示为 Toeplitz矩阵乘向量：c = T(a) * b
// - 矩阵-向量乘法复杂度 O(n²)，与basic_mul相同
// - 标准矩阵乘法 C=A×B 是三矩阵运算，大数乘法只有两输入，无法直接映射
// - 即使分块，复杂度仍为O(n²)
//
// 本原型验证：
// 1. Toeplitz矩阵乘向量的正确性
// 2. Toeplitz vs 直接卷积的性能对比
// 3. 分块矩阵乘法能否利用SIMD FMA加速

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

using namespace std;

// ============ 直接卷积 (参考) ============
void direct_conv(const int* a, size_t na, const int* b, size_t nb, vector<long long>& out) {
    if (na == 0 || nb == 0) return;
    out.assign(na + nb - 1, 0);
    for (size_t i = 0; i < na; i++)
        for (size_t j = 0; j < nb; j++)
            out[i + j] += (long long)a[i] * b[j];
}

// ============ Toeplitz矩阵乘向量 ============
// T(a) 是 (na+nb-1) × nb 矩阵，T(a)[i][j] = (i-j < na && i-j >= 0) ? a[i-j] : 0
// c = T(a) * b，c[i] = sum_j T(a)[i][j] * b[j] = sum_j a[i-j] * b[j]
void toeplitz_mul(const int* a, size_t na, const int* b, size_t nb, vector<long long>& out) {
    if (na == 0 || nb == 0) return;
    size_t nc = na + nb - 1;
    out.assign(nc, 0);
    // 构造Toeplitz矩阵并乘b（等价于直接卷积）
    for (size_t i = 0; i < nc; i++) {
        long long sum = 0;
        for (size_t j = 0; j < nb; j++) {
            long long diff = (long long)i - j;
            if (diff >= 0 && diff < (long long)na) {
                sum += (long long)a[diff] * b[j];
            }
        }
        out[i] = sum;
    }
}

// ============ 分块Toeplitz矩阵乘 (模拟4×4分块SIMD) ============
// 将Toeplitz矩阵分为4×4块，每块做矩阵乘法
// 理论上可用AVX2 FMA加速，但本质仍是O(n²)
void blocked_toeplitz_mul(const int* a, size_t na, const int* b, size_t nb, vector<long long>& out) {
    if (na == 0 || nb == 0) return;
    size_t nc = na + nb - 1;
    out.assign(nc, 0);
    // 分块：每块4×4
    const size_t BLOCK = 4;
    for (size_t ii = 0; ii < nc; ii += BLOCK) {
        for (size_t jj = 0; jj < nb; jj += BLOCK) {
            // 处理4×4块
            long long acc[BLOCK][BLOCK] = {{0}};
            for (size_t di = 0; di < BLOCK && ii + di < nc; di++) {
                for (size_t dj = 0; dj < BLOCK && jj + dj < nb; dj++) {
                    size_t i = ii + di, j = jj + dj;
                    long long diff = (long long)i - j;
                    if (diff >= 0 && diff < (long long)na) {
                        acc[di][dj] = (long long)a[diff] * b[j];
                    }
                }
            }
            // 累加到out
            for (size_t di = 0; di < BLOCK && ii + di < nc; di++) {
                for (size_t dj = 0; dj < BLOCK && jj + dj < nb; dj++) {
                    out[ii + di] += acc[di][dj];
                }
            }
        }
    }
}

int main() {
    printf("=== 自研算法方向D：分块SIMD矩阵乘法 ===\n\n");

    // 1. 正确性验证
    printf("--- 正确性验证 ---\n");
    mt19937 rng(42);
    bool all_pass = true;
    for (int t = 0; t < 50; t++) {
        size_t na = 1 + rng() % 60;
        size_t nb = 1 + rng() % 60;
        vector<int> a(na), b(nb);
        for (size_t i = 0; i < na; i++) a[i] = rng() % 10000;
        for (size_t i = 0; i < nb; i++) b[i] = rng() % 10000;

        vector<long long> out_direct, out_toep, out_blocked;
        direct_conv(a.data(), na, b.data(), nb, out_direct);
        toeplitz_mul(a.data(), na, b.data(), nb, out_toep);
        blocked_toeplitz_mul(a.data(), na, b.data(), nb, out_blocked);

        if (out_direct != out_toep) {
            printf("  FAIL t=%d: direct != toeplitz\n", t); all_pass = false; break;
        }
        if (out_direct != out_blocked) {
            printf("  FAIL t=%d: direct != blocked\n", t); all_pass = false; break;
        }
    }
    printf("  %s\n\n", all_pass ? "50 cases PASS" : "FAIL");
    if (!all_pass) { printf("CORRECTNESS FAILED\n"); return 1; }

    // 2. 性能对比
    printf("--- 性能对比 (每轮耗时 ms) ---\n");
    printf("  %8s %8s %12s %12s %12s %10s %10s\n", "N", "iters", "direct", "toeplitz", "blocked", "toep/dir", "blk/dir");
    fflush(stdout);

    size_t N_list[] = {8, 16, 32, 64, 128, 256};
    int iters_list[] = {5000, 2000, 1000, 500, 200, 100};

    for (int idx = 0; idx < 6; idx++) {
        size_t n = N_list[idx];
        int iters = iters_list[idx];
        vector<int> a(n), b(n);
        for (size_t i = 0; i < n; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }
        vector<long long> out;
        volatile long long sink = 0;

        // direct
        auto t0 = chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { direct_conv(a.data(), n, b.data(), n, out); sink += out[0]; }
        auto t1 = chrono::high_resolution_clock::now();
        double t_direct = chrono::duration<double, milli>(t1 - t0).count() / iters;

        // toeplitz
        t0 = chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { toeplitz_mul(a.data(), n, b.data(), n, out); sink += out[0]; }
        t1 = chrono::high_resolution_clock::now();
        double t_toep = chrono::duration<double, milli>(t1 - t0).count() / iters;

        // blocked
        t0 = chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; i++) { blocked_toeplitz_mul(a.data(), n, b.data(), n, out); sink += out[0]; }
        t1 = chrono::high_resolution_clock::now();
        double t_blocked = chrono::duration<double, milli>(t1 - t0).count() / iters;

        printf("  %8zu %8d %12.4f %12.4f %12.4f %10.3f %10.3f\n",
               n, iters, t_direct, t_toep, t_blocked, t_toep / t_direct, t_blocked / t_direct);
        fflush(stdout);
    }

    printf("\n=== 结论 ===\n");
    printf("1. 大数乘法(卷积) = Toeplitz矩阵乘向量，不是标准矩阵乘法\n");
    printf("2. Toeplitz矩阵乘向量复杂度O(n²)，与直接卷积相同\n");
    printf("3. 分块矩阵乘法无法降低复杂度（仍为O(n²)）\n");
    printf("4. 标准矩阵乘法C=A×B是三矩阵运算，大数乘法只有两输入，无法直接映射\n");
    printf("5. 即使SIMD化，O(n²)仍比FFT的O(n log n)慢\n");
    printf("方向D理论证伪：矩阵范式无法优于FFT。\n");
    return 0;
}
