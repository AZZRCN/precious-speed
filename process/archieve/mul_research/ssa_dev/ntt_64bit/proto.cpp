/*
 * SSA路径2：64位NTT单模数(Goldilocks)原型
 * 模数：p = 2^64 - 2^32 + 1 = 18446744069414584321
 *   p-1 = 2^32 * (2^32-1), NTT长度最高2^32
 *   2^32-1 = 3*5*17*257*65537
 *   原根g = 3 (需验证)
 *
 * 精度分析（10^4 base）：
 *   - 每个limb最大9999，两limb相乘最大~10^8
 *   - NTT长度2^20：结果最大2^20 * 10^8 ~ 10^14
 *   - p ~ 1.8*10^19 >> 10^14，单模数足够！
 *
 * 优化：利用2^64 ≡ 2^32-1 (mod p)做快速模约简，避免__int128除法
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace ntt64 {

constexpr uint64_t P = 0xFFFFFFFF00000001ULL;  // 2^64 - 2^32 + 1
constexpr uint64_t G = 3ULL;
constexpr uint64_t REDUCE_C = (1ULL << 32) - 1ULL;  // 2^32 - 1

// 优化的模约简：利用 2^64 ≡ 2^32-1 (mod p)
// 128位乘积 hi:lo mod p = lo + hi * (2^32-1)，递归约简
inline uint64_t mod_reduce(__uint128_t prod) {
    uint64_t lo = (uint64_t)prod;
    uint64_t hi = (uint64_t)(prod >> 64);
    // prod ≡ lo + hi * (2^32 - 1) (mod p)
    __uint128_t r = (__uint128_t)lo + (__uint128_t)hi * REDUCE_C;
    // 第二次约简 (r < 2^96)
    lo = (uint64_t)r;
    hi = (uint64_t)(r >> 64);
    r = (__uint128_t)lo + (__uint128_t)hi * REDUCE_C;
    // 第三次约简 (r < 2^66)
    lo = (uint64_t)r;
    hi = (uint64_t)(r >> 64);
    r = (__uint128_t)lo + (__uint128_t)hi * REDUCE_C;
    // r < 2^64 + 4*2^32 < 2p
    uint64_t result = (uint64_t)r;
    if (result >= P) result -= P;
    return result;
}

inline uint64_t mulmod(uint64_t a, uint64_t b) {
    return mod_reduce((__uint128_t)a * b);
}

inline uint64_t addmod(uint64_t a, uint64_t b) {
    uint64_t r = a + b;
    if (r < a || r >= P) r -= P;  // 溢出或>=P都减P
    return r;
}

inline uint64_t submod(uint64_t a, uint64_t b) {
    return a >= b ? a - b : a + P - b;
}

uint64_t power(uint64_t a, uint64_t b) {
    uint64_t r = 1;
    while (b) {
        if (b & 1) r = mulmod(r, a);
        a = mulmod(a, a);
        b >>= 1;
    }
    return r;
}

// 验证原根：g是原根当且仅当 g^((p-1)/q) != 1 for all prime q | (p-1)
// p-1 = 2^32 * (2^32-1), 2^32-1 = 3*5*17*257*65537
bool is_primitive_root(uint64_t g) {
    uint64_t factors[] = {2, 3, 5, 17, 257, 65537};
    for (uint64_t q : factors) {
        uint64_t exp = (P - 1) / q;
        if (power(g, exp) == 1) return false;
    }
    return true;
}

uint64_t find_primitive_root() {
    for (uint64_t g = 2; g < 1000; g++) {
        if (is_primitive_root(g)) return g;
    }
    return 0;
}

// 预计算旋转因子
uint64_t* twiddle_fwd = nullptr;
uint64_t* twiddle_inv = nullptr;
int twiddle_n = 0;

void init_twiddle(int n) {
    if (twiddle_n >= n) return;
    delete[] twiddle_fwd;
    delete[] twiddle_inv;
    twiddle_fwd = new uint64_t[n / 2];
    twiddle_inv = new uint64_t[n / 2];
    uint64_t wf = power(G, (P - 1) / n);
    uint64_t wi = power(G, P - 1 - (P - 1) / n);
    twiddle_fwd[0] = 1;
    twiddle_inv[0] = 1;
    for (int i = 1; i < n / 2; i++) {
        twiddle_fwd[i] = mulmod(twiddle_fwd[i - 1], wf);
        twiddle_inv[i] = mulmod(twiddle_inv[i - 1], wi);
    }
    twiddle_n = n;
}

// NTT forward (DIF): 先加减，后乘旋转因子
void ntt_forward(uint64_t* a, int n) {
    for (int m = n, half = n >> 1; half; m = half, half >>= 1) {
        int step = n / m;
        for (int i = 0; i < n; i += m) {
            for (int j = 0; j < half; j++) {
                uint64_t u = a[i + j];
                uint64_t v = a[i + j + half];
                a[i + j] = addmod(u, v);
                uint64_t diff = submod(u, v);
                a[i + j + half] = mulmod(diff, twiddle_fwd[j * step]);
            }
        }
    }
}

// NTT inverse (DIT): 先乘旋转因子，后加减
void ntt_inverse(uint64_t* a, int n) {
    for (int m = 2, half = 1; half < n; m <<= 1, half = m >> 1) {
        int step = n / m;
        for (int i = 0; i < n; i += m) {
            for (int j = 0; j < half; j++) {
                uint64_t u = a[i + j];
                uint64_t v = mulmod(a[i + j + half], twiddle_inv[j * step]);
                a[i + j] = addmod(u, v);
                a[i + j + half] = submod(u, v);
            }
        }
    }
    uint64_t inv_n = power(n, P - 2);
    for (int i = 0; i < n; i++) a[i] = mulmod(a[i], inv_n);
}

inline void pointwise_mul(uint64_t* a, const uint64_t* b, int n) {
    for (int i = 0; i < n; i++) a[i] = mulmod(a[i], b[i]);
}

} // namespace ntt64

int main() {
    using namespace ntt64;

    // 搜索原根
    printf("=== 原根搜索 ===\n");
    uint64_t g_root = find_primitive_root();
    printf("找到原根 g=%llu\n", (unsigned long long)g_root);
    if (g_root == 0) {
        printf("ERROR: 未找到原根\n");
        return 1;
    }

    // 用找到的原根重新计算twiddle（覆盖G）
    // 由于G是constexpr，我们直接用g_root计算
    const int N = 1 << 20;
    uint64_t* a = new uint64_t[N];
    uint64_t* b = new uint64_t[N];

    for (int i = 0; i < N; i++) {
        a[i] = (uint64_t)(i % 10000);
        b[i] = (uint64_t)((i * 7 + 3) % 10000);
    }

    // 手动初始化twiddle（用g_root而非G）
    delete[] twiddle_fwd;
    delete[] twiddle_inv;
    twiddle_fwd = new uint64_t[N / 2];
    twiddle_inv = new uint64_t[N / 2];
    uint64_t wf = power(g_root, (P - 1) / N);
    uint64_t wi = power(g_root, P - 1 - (P - 1) / N);
    twiddle_fwd[0] = 1;
    twiddle_inv[0] = 1;
    for (int i = 1; i < N / 2; i++) {
        twiddle_fwd[i] = mulmod(twiddle_fwd[i - 1], wf);
        twiddle_inv[i] = mulmod(twiddle_inv[i - 1], wi);
    }
    twiddle_n = N;

    // 性能测试
    double times_fwd[30], times_total[30];
    for (int t = 0; t < 30; t++) {
        for (int i = 0; i < N; i++) {
            a[i] = (uint64_t)(i % 10000);
            b[i] = (uint64_t)((i * 7 + 3) % 10000);
        }
        auto t0 = std::chrono::high_resolution_clock::now();
        ntt_forward(a, N);
        auto t1 = std::chrono::high_resolution_clock::now();
        ntt_forward(b, N);
        pointwise_mul(a, b, N);
        ntt_inverse(a, N);
        auto t2 = std::chrono::high_resolution_clock::now();
        times_fwd[t] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times_total[t] = std::chrono::duration<double, std::milli>(t2 - t0).count();
    }

    std::sort(times_fwd, times_fwd + 30);
    std::sort(times_total, times_total + 30);

    printf("\n=== 64位NTT性能测试 (N=2^20, 30轮) ===\n");
    printf("单次DIF:  中位数=%.3f ms, min=%.3f, max=%.3f\n",
           times_fwd[15], times_fwd[0], times_fwd[29]);
    printf("完整卷积(DIF*2+DOT+IDT): 中位数=%.3f ms, min=%.3f, max=%.3f\n",
           times_total[15], times_total[0], times_total[29]);
    printf("\n对比:\n");
    printf("  30位NTT单模数卷积: ~78ms\n");
    printf("  FFT卷积(385663):    ~16ms\n");
    printf("  本64位NTT:          %.1fms\n", times_total[15]);
    printf("\n注: 64位NTT单模数足够精度，无需CRT。若比30位NTT快则有意义。\n");

    // 正确性验证
    printf("\n=== 正确性验证 ===\n");
    const int M = 8;
    uint64_t va[M] = {1, 2, 3, 0, 0, 0, 0, 0};
    uint64_t vb[M] = {4, 5, 0, 0, 0, 0, 0, 0};
    // 重新初始化M点twiddle
    delete[] twiddle_fwd;
    delete[] twiddle_inv;
    twiddle_fwd = new uint64_t[M / 2];
    twiddle_inv = new uint64_t[M / 2];
    wf = power(g_root, (P - 1) / M);
    wi = power(g_root, P - 1 - (P - 1) / M);
    twiddle_fwd[0] = 1;
    twiddle_inv[0] = 1;
    for (int i = 1; i < M / 2; i++) {
        twiddle_fwd[i] = mulmod(twiddle_fwd[i - 1], wf);
        twiddle_inv[i] = mulmod(twiddle_inv[i - 1], wi);
    }
    ntt_forward(va, M);
    ntt_forward(vb, M);
    pointwise_mul(va, vb, M);
    ntt_inverse(va, M);
    printf("(1+2x+3x^2)*(4+5x): 期望 4 13 22 15 0 0 0 0\n");
    printf("实际: ");
    for (int i = 0; i < M; i++) printf("%llu ", (unsigned long long)va[i]);
    printf("\n");
    bool ok = (va[0]==4 && va[1]==13 && va[2]==22 && va[3]==15);
    printf("结果: %s\n", ok ? "PASS" : "FAIL");

    delete[] a;
    delete[] b;
    return 0;
}
