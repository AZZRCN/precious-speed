// 自研算法方向B：基于DCT的实数乘法
// 核心验证：DCT的卷积性质（对称卷积）能否用于标准卷积（大数乘法）
//
// 理论分析：
// - DFT卷积定理：DFT(a*b) = DFT(a)·DFT(b)，*为循环卷积 → 可用于大数乘法
// - DCT卷积定理：DCT对应"对称卷积"(symmetric convolution) ≠ 标准卷积
// - 要用DCT做标准卷积需对称延拓，但延拓后长度翻倍，抵消DCT的数据量优势
//
// 本原型验证：
// 1. DCT-II直接点积 → IDCT 是否等于标准卷积（预期：不等）
// 2. 对称延拓后DCT → 点积 → IDCT 是否等于标准卷积（预期：等，但长度翻倍）

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

using namespace std;

// ============ DCT-II (O(N²)) ============
void dct_ii(const vector<double>& x, vector<double>& X) {
    size_t N = x.size();
    X.resize(N);
    for (size_t k = 0; k < N; k++) {
        double sum = 0;
        for (size_t n = 0; n < N; n++) {
            sum += x[n] * cos(M_PI * (2 * n + 1) * k / (2 * N));
        }
        X[k] = sum;
    }
}

// ============ IDCT (DCT-III, 带归一化) ============
void idct(const vector<double>& X, vector<double>& x) {
    size_t N = X.size();
    x.resize(N);
    for (size_t n = 0; n < N; n++) {
        double sum = X[0] * 0.5;
        for (size_t k = 1; k < N; k++) {
            sum += X[k] * cos(M_PI * (2 * n + 1) * k / (2 * N));
        }
        x[n] = sum * 2.0 / N;
    }
}

// ============ 标准线性卷积 (参考) ============
void linear_conv(const vector<double>& a, const vector<double>& b, vector<double>& out) {
    size_t na = a.size(), nb = b.size();
    out.assign(na + nb - 1, 0);
    for (size_t i = 0; i < na; i++)
        for (size_t j = 0; j < nb; j++)
            out[i + j] += a[i] * b[j];
}

// ============ 方法1: DCT直接点积 (预期失败) ============
void dct_mul_naive(const vector<double>& a, const vector<double>& b, vector<double>& out) {
    size_t na = a.size(), nb = b.size();
    size_t L = 1;
    while (L < na + nb - 1) L <<= 1;
    vector<double> a_pad(L, 0), b_pad(L, 0);
    for (size_t i = 0; i < na; i++) a_pad[i] = a[i];
    for (size_t i = 0; i < nb; i++) b_pad[i] = b[i];

    vector<double> A, B;
    dct_ii(a_pad, A);
    dct_ii(b_pad, B);

    vector<double> C(L);
    for (size_t i = 0; i < L; i++) C[i] = A[i] * B[i];

    vector<double> result;
    idct(C, result);

    out.assign(na + nb - 1, 0);
    for (size_t i = 0; i < na + nb - 1; i++) out[i] = result[i];
    // 归一化
    double norm = 2.0 / L;
    for (size_t i = 0; i < out.size(); i++) out[i] *= norm * L / 2;
}

// ============ 方法2: 对称延拓后DCT (预期正确但长度翻倍) ============
void dct_mul_symmetric(const vector<double>& a, const vector<double>& b, vector<double>& out) {
    size_t na = a.size(), nb = b.size();
    size_t L = 1;
    while (L < na + nb - 1) L <<= 1;
    // 对称延拓：a_pad长度2L
    // a_pad[0..L-1] = a[0..na-1] 补零到L
    // a_pad[L..2L-1] = a_pad[2L-1-n] 的镜像
    vector<double> a_pad(2 * L, 0), b_pad(2 * L, 0);
    for (size_t i = 0; i < na; i++) a_pad[i] = a[i];
    for (size_t i = 0; i < nb; i++) b_pad[i] = b[i];
    // 镜像延拓
    for (size_t i = 0; i < L; i++) {
        a_pad[2 * L - 1 - i] = a_pad[i];
        b_pad[2 * L - 1 - i] = b_pad[i];
    }

    vector<double> A, B;
    dct_ii(a_pad, A);
    dct_ii(b_pad, B);

    vector<double> C(2 * L);
    for (size_t i = 0; i < 2 * L; i++) C[i] = A[i] * B[i];

    vector<double> result;
    idct(C, result);

    out.assign(na + nb - 1, 0);
    for (size_t i = 0; i < na + nb - 1; i++) out[i] = result[i];
    // 归一化
    for (size_t i = 0; i < out.size(); i++) out[i] /= (2 * L);
}

int main() {
    printf("=== 自研算法方向B：DCT乘法验证 ===\n\n");

    mt19937 rng(42);
    int total_pass_naive = 0, total_pass_sym = 0, total_tests = 20;

    for (int t = 0; t < total_tests; t++) {
        size_t na = 3 + rng() % 8;
        size_t nb = 3 + rng() % 8;
        vector<double> a(na), b(nb);
        for (size_t i = 0; i < na; i++) a[i] = rng() % 100;
        for (size_t i = 0; i < nb; i++) b[i] = rng() % 100;

        vector<double> ref, naive_res, sym_res;
        linear_conv(a, b, ref);
        dct_mul_naive(a, b, naive_res);
        dct_mul_symmetric(a, b, sym_res);

        // 比较方法1
        double err_naive = 0;
        for (size_t i = 0; i < ref.size(); i++)
            err_naive = max(err_naive, abs(ref[i] - naive_res[i]));
        bool pass_naive = (err_naive < 0.5);

        // 比较方法2
        double err_sym = 0;
        for (size_t i = 0; i < ref.size(); i++)
            err_sym = max(err_sym, abs(ref[i] - sym_res[i]));
        bool pass_sym = (err_sym < 0.5);

        if (pass_naive) total_pass_naive++;
        if (pass_sym) total_pass_sym++;

        if (t < 3 || (!pass_naive && !pass_sym)) {
            printf("  t=%d na=%zu nb=%zu\n", t, na, nb);
            printf("    ref:    ");
            for (size_t i = 0; i < min(ref.size(), (size_t)6); i++) printf("%.0f ", ref[i]);
            printf("\n    naive:  ");
            for (size_t i = 0; i < min(naive_res.size(), (size_t)6); i++) printf("%.1f ", naive_res[i]);
            printf(" err=%.2f %s\n", err_naive, pass_naive ? "PASS" : "FAIL");
            printf("    sym:    ");
            for (size_t i = 0; i < min(sym_res.size(), (size_t)6); i++) printf("%.1f ", sym_res[i]);
            printf(" err=%.2f %s\n", err_sym, pass_sym ? "PASS" : "FAIL");
        }
    }

    printf("\n=== 结论 ===\n");
    printf("方法1 (DCT直接点积): %d/%d PASS\n", total_pass_naive, total_tests);
    printf("  -> DCT对称卷积 %s 标准卷积\n",
           total_pass_naive == total_tests ? "等于" : "不等于");
    printf("方法2 (对称延拓DCT): %d/%d PASS\n", total_pass_sym, total_tests);
    printf("  -> 对称延拓后DCT %s 计算标准卷积\n",
           total_pass_sym == total_tests ? "可正确" : "无法正确");
    printf("\n理论结论：\n");
    printf("  - DCT的卷积性质对应'对称卷积'，不是标准卷积\n");
    printf("  - 要用DCT做标准卷积需对称延拓(长度2L)，但DFT只需长度L\n");
    printf("  - DCT计算量 >= DFT计算量（DCT可用2L点DFT实现）\n");
    printf("  - RIRI打包已实现实数序列的高效DFT，DCT无额外优势\n");
    return 0;
}
