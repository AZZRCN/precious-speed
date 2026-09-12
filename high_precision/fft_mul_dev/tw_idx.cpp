// tw_idx.cpp —— 实测各尺寸下 twiddle 最大索引, 确定大表真实容量需求
// g++ -O2 -march=haswell -std=c++17 -DTW_TRACE tw_idx.cpp -o tw_idx
#define TW_TRACE 1
#include "prod_fft1.h"
#include <cstdio>
#include <vector>
#include <random>

using cpx = __m128d;

int main() {
    printf("%-8s %-8s %-12s %-12s %-10s\n", "n", "maxidx", "maxidx/n", "flat_bytes", "twobase_B");
    for (int bits = 8; bits <= 22; ++bits) {
        u32 n = 1u << bits;
        fft_flat::resize(n);
        std::vector<double> raw(2 * (size_t)n);
        std::mt19937_64 rng(99);
        for (auto& d : raw) d = (double)(rng() & 0xffff) / 65536.0 - 0.5;
        cpx* d = (cpx*)raw.data();

        fft_flat::twMaxIdx = 0;
        fft_flat::difRec(d, n, 0);
        u32 m1 = fft_flat::twMaxIdx;
        fft_flat::twMaxIdx = 0;
        fft_flat::ditRec(d, n, 0);
        u32 m2 = fft_flat::twMaxIdx;
        fft_flat::twMaxIdx = 0;
        fft_flat::pointwiseSq(d, n);
        u32 m3 = fft_flat::twMaxIdx;
        u32 mx = m1 > m2 ? m1 : m2; if (m3 > mx) mx = m3;
        printf("2^%-6d %-8u %-12.4f %-12.2f %-10.2f\n", bits, mx, (double)mx / n,
               (double)(mx + 1) * 16.0 / 1048576.0, 65536.0 / 1048576.0);
    }
    return 0;
}
