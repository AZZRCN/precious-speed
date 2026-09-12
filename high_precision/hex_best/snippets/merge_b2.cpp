// ===== merge_b2.cpp =====
// 合同: split_b2 的逆运算。把 FFT 输出的 base-2^k double 数字流 g[] 进位折叠回 n 个 base-2^64 limbs,
//       写入 f[0..n)。g[j]+0.5 四舍五入取整 -> 数字; 按位宽 k 累积, 满 64 位吐出一个 limb。
// HOT: 每 fm_mul 必跑, O(total) 且 total=ceil(n*64/k)≈3.4n。当前为标量进位链 -->
//       **SWAR/AVX2 超优化首选**(参考 DEC D19 absMul1: _pext 提取进位掩码 + SWAR 涟漪链一次 16 元素)。
//       注意: g[j] 可能含 FFT 舍入误差(±0.5 内), +0.5 四舍五入必须保留; 不要假设数字已精确整数。
#include <cstdint>
using u64 = uint64_t;
using u128 = unsigned __int128;

static void merge_b2(u64* f, const double* g, size_t n, int k) {
    size_t i = 0, j = 0;
    int w = 0;
    u128 tmp = 0;
    while (i < n) {
        while (w < 64) { tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }
        f[i++] = (u64)tmp;
        tmp >>= 64, w -= 64;
    }
}
