/*
 * SSA路径1：30位NTT多模数+CRT原型
 * 目标：测量2^20点NTT的基础性能，对比FFT
 * 模数：998244353 = 119 * 2^23 + 1, 原根3
 *
 * 精度分析（10^4 base）：
 *   - 每个limb最大9999，两limb相乘最大~10^8
 *   - NTT长度2^20：结果最大2^20 * 10^8 ~ 10^14
 *   - 单模数P~10^9不够，需3模数CRT（精度~10^27 >> 10^14）
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <algorithm>

namespace ntt30 {

constexpr uint32_t P = 998244353u;   // 119 * 2^23 + 1
constexpr uint32_t G = 3u;

inline uint32_t power(uint32_t a, uint32_t b) {
    uint64_t r = 1, a64 = a;
    while (b) {
        if (b & 1) r = r * a64 % P;
        a64 = a64 * a64 % P;
        b >>= 1;
    }
    return (uint32_t)r;
}

// 预计算旋转因子表
uint32_t* twiddle_fwd = nullptr;  // G^((P-1)/N * k), k=0..N/2-1
uint32_t* twiddle_inv = nullptr;  // G^((P-1-(P-1)/N) * k)
int twiddle_n = 0;

void init_twiddle(int n) {
    if (twiddle_n >= n) return;
    delete[] twiddle_fwd;
    delete[] twiddle_inv;
    twiddle_fwd = new uint32_t[n / 2];
    twiddle_inv = new uint32_t[n / 2];
    uint32_t wf = power(G, (P - 1) / n);
    uint32_t wi = power(G, P - 1 - (P - 1) / n);
    twiddle_fwd[0] = 1;
    twiddle_inv[0] = 1;
    for (int i = 1; i < n / 2; i++) {
        twiddle_fwd[i] = (uint32_t)((uint64_t)twiddle_fwd[i - 1] * wf % P);
        twiddle_inv[i] = (uint32_t)((uint64_t)twiddle_inv[i - 1] * wi % P);
    }
    twiddle_n = n;
}

// NTT forward (DIF), in-place
// DIF蝶形：先加减，后乘旋转因子
//   a[j]      = u + v
//   a[j+half] = (u - v) * W^j
void ntt_forward(uint32_t* a, int n) {
    for (int m = n, half = n >> 1; half; m = half, half >>= 1) {
        int step = n / m;
        for (int i = 0; i < n; i += m) {
            const uint32_t* tw = twiddle_fwd;
            for (int j = 0; j < half; j++) {
                uint32_t u = a[i + j];
                uint32_t v = a[i + j + half];
                uint32_t sum = u + v;
                if (sum >= P) sum -= P;
                uint32_t diff = u >= v ? u - v : u + P - v;
                a[i + j] = sum;
                a[i + j + half] = (uint32_t)((uint64_t)diff * tw[j * step] % P);
            }
        }
    }
}

// NTT inverse (DIT), in-place, includes 1/N scaling
// DIT蝶形：先乘旋转因子，后加减
//   u = a[j], v = a[j+half] * W^(-j)
//   a[j]      = u + v
//   a[j+half] = u - v
void ntt_inverse(uint32_t* a, int n) {
    for (int m = 2, half = 1; half < n; m <<= 1, half = m >> 1) {
        int step = n / m;
        for (int i = 0; i < n; i += m) {
            const uint32_t* tw = twiddle_inv;
            for (int j = 0; j < half; j++) {
                uint32_t u = a[i + j];
                uint32_t v = (uint32_t)((uint64_t)a[i + j + half] * tw[j * step] % P);
                uint32_t sum = u + v;
                if (sum >= P) sum -= P;
                uint32_t diff = u >= v ? u - v : u + P - v;
                a[i + j] = sum;
                a[i + j + half] = diff;
            }
        }
    }
    uint32_t inv_n = power(n, P - 2);
    for (int i = 0; i < n; i++) a[i] = (uint32_t)((uint64_t)a[i] * inv_n % P);
}

// 点积
inline void pointwise_mul(uint32_t* a, const uint32_t* b, int n) {
    for (int i = 0; i < n; i++) a[i] = (uint32_t)((uint64_t)a[i] * b[i] % P);
}

} // namespace ntt30

int main() {
    using namespace ntt30;
    const int N = 1 << 20;  // 2^20
    
    uint32_t* a = new uint32_t[N];
    uint32_t* b = new uint32_t[N];
    
    // 初始化测试数据（模拟10^4 base的limbs）
    for (int i = 0; i < N; i++) {
        a[i] = (uint32_t)(i % 10000);
        b[i] = (uint32_t)((i * 7 + 3) % 10000);
    }
    
    init_twiddle(N);
    
    // 性能测试：30次循环取中位数
    double times_fwd[30], times_total[30];
    
    for (int t = 0; t < 30; t++) {
        // 重置数据
        for (int i = 0; i < N; i++) {
            a[i] = (uint32_t)(i % 10000);
            b[i] = (uint32_t)((i * 7 + 3) % 10000);
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
    
    printf("=== 30位NTT性能测试 (N=2^20, 30轮) ===\n");
    printf("单次DIF:  中位数=%.3f ms, min=%.3f, max=%.3f\n", 
           times_fwd[15], times_fwd[0], times_fwd[29]);
    printf("完整卷积(DIF*2+DOT+IDT): 中位数=%.3f ms, min=%.3f, max=%.3f\n", 
           times_total[15], times_total[0], times_total[29]);
    printf("\n对比参考（385663 FFT, max_max_00本地）:\n");
    printf("  完整乘法端到端: ~44ms (含I/O)\n");
    printf("  FFT卷积(DIF*2+DOT+IDT): ~16ms (profile数据)\n");
    printf("\n注: 此为单模数NTT，实际需3模数+CRT，总时间约3x\n");
    
    // 正确性验证（小规模）
    printf("\n=== 正确性验证 ===\n");
    const int M = 8;
    uint32_t va[M] = {1, 2, 3, 0, 0, 0, 0, 0};  // 1 + 2x + 3x^2
    uint32_t vb[M] = {4, 5, 0, 0, 0, 0, 0, 0};  // 4 + 5x
    // 期望结果: 4 + 13x + 22x^2 + 15x^3

    twiddle_n = 0;  // 强制重新初始化旋转因子
    init_twiddle(M);
    ntt_forward(va, M);
    ntt_forward(vb, M);
    pointwise_mul(va, vb, M);
    ntt_inverse(va, M);
    
    printf("多项式乘法 (1+2x+3x^2)*(4+5x):\n");
    printf("期望: 4 13 22 15 0 0 0 0\n");
    printf("实际: ");
    for (int i = 0; i < M; i++) printf("%u ", va[i]);
    printf("\n");
    
    bool ok = (va[0]==4 && va[1]==13 && va[2]==22 && va[3]==15);
    printf("结果: %s\n", ok ? "PASS" : "FAIL");
    
    delete[] a;
    delete[] b;
    return 0;
}
