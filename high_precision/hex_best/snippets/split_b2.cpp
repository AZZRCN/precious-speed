// ===== split_b2.cpp =====
// 合同: 把 n 个 base-2^64 limbs 拆成 FFT 所需的 base-2^k 数字(double), 写入 g[0..zlim)。
//       total = ceil(n*64/k); zlim >= total(通常 = FFT 长度 lm), 尾部补零由本函数负责。
//       k in [8,19] 由 pick_k 选定(精度预算 d*2^(2k)<=2^47)。写满 g[0..zlim) 供后续 FFT 原地变换。
// 当前实现: AVX2 gather 处理对齐段 + 标量尾。优化方向: 用更宽 gather/permutexvar 替代
//           i32gather, 减少 cvtepi32_pd 两条; 或用 shuffle 网络直接产出 double。
#include <cstdint>
#include <cstring>
#include <immintrin.h>
using u64 = uint64_t;
using u32 = uint32_t;

static size_t split_b2(const u64* src, double* g, size_t n, int k, size_t zlim) {
    const size_t total = ((n << 6) + (size_t)k - 1) / (size_t)k;
    const unsigned char* base = (const unsigned char*)src;
    const u32 m = (1u << k) - 1;
    size_t j = 0;
    // 向量段: 保证 gather 读取的 4 字节完全落在 [0, 8n)
    size_t vend = ((n << 6) >= 32) ? ((n << 6) - 32) / (size_t)k : 0;
    if (vend > total) vend = total;
    vend &= ~(size_t)7;
    if (vend) {
        const __m256i off0 = _mm256_setr_epi32(0, k, 2 * k, 3 * k, 4 * k, 5 * k, 6 * k, 7 * k);
        const __m256i step = _mm256_set1_epi32(8 * k);
        const __m256i mv = _mm256_set1_epi32((int)m);
        const __m256i m7 = _mm256_set1_epi32(7);
        __m256i bit = off0;
        for (; j < vend; j += 8) {
            __m256i bo = _mm256_srli_epi32(bit, 3);
            __m256i sh = _mm256_and_si256(bit, m7);
            __m256i w = _mm256_i32gather_epi32((const int*)base, bo, 1);
            w = _mm256_and_si256(_mm256_srlv_epi32(w, sh), mv);
            _mm256_storeu_pd(g + j, _mm256_cvtepi32_pd(_mm256_castsi256_si128(w)));
            _mm256_storeu_pd(g + j + 4, _mm256_cvtepi32_pd(_mm256_extracti128_si256(w, 1)));
            bit = _mm256_add_epi32(bit, step);
        }
    }
    for (; j < total; ++j) {  // 安全标量尾 (末尾高位补 0)
        const size_t b = j * (size_t)k;
        const size_t li = b >> 6;
        const int sh = (int)(b & 63);
        u64 v = (li < n) ? (src[li] >> sh) : 0;
        if (sh && li + 1 < n) v |= src[li + 1] << (64 - sh);
        g[j] = (double)(u32)(v & m);
    }
    // 只清尾部: 前 total 项已被写满, 全量 memset 是冗余双写
    if (zlim > total) std::memset(g + total, 0, (zlim - total) * 8);
    return total;
}
