// radix-3 档位成本曲线: 对每个 k, 比较 real_conv(2^k) 与 real_conv(3*2^(k-2))
// 后者是前者在 fft_ceil 下的替代品。ratio<1 才算赚。
#define main hint_div_main_disabled
#include "div_D12.cpp"
#undef main

#include <cstdio>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>

using clk = std::chrono::high_resolution_clock;

static double timeit(size_t FL, int iters)
{
    hint::AlignedVec32<double> a(FL), b(FL);
    std::mt19937_64 rng(1234 + FL);
    for (size_t i = 0; i < FL; i++)
        a[i] = double(rng() % 10000), b[i] = double(rng() % 10000);
    hint::AlignedVec32<double> ta(FL), tb(FL);
    // 预热
    std::copy(a.begin(), a.end(), ta.begin());
    std::copy(b.begin(), b.end(), tb.begin());
    hint::transform::fft::real_conv(ta.data(), tb.data(), FL);

    std::vector<double> samples;
    for (int r = 0; r < iters; r++)
    {
        std::copy(a.begin(), a.end(), ta.begin());
        std::copy(b.begin(), b.end(), tb.begin());
        auto t0 = clk::now();
        hint::transform::fft::real_conv(ta.data(), tb.data(), FL);
        auto t1 = clk::now();
        samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

int main()
{
    printf("  k     FL2       t2(ms)      FL3       t3(ms)   t3/t2   perpoint(t3/FL3)/(t2/FL2)\n");
    for (int k = 8; k <= 22; k++)
    {
        size_t FL2 = size_t(1) << k;
        size_t FL3 = FL2 / 4 * 3;
        int iters = FL2 >= (1u << 20) ? 5 : (FL2 >= (1u << 16) ? 15 : 60);
        // 交替测量, 避免频率漂移偏置
        double t2 = 0, t3 = 0;
        for (int rep = 0; rep < 3; rep++)
        {
            double x2 = timeit(FL2, iters);
            double x3 = timeit(FL3, iters);
            if (rep == 0 || x2 < t2)
                t2 = x2;
            if (rep == 0 || x3 < t3)
                t3 = x3;
        }
        printf("%3d  %8zu  %9.4f  %8zu  %9.4f   %6.3f   %6.3f\n",
               k, FL2, t2, FL3, t3, t3 / t2, (t3 / FL3) / (t2 / FL2));
    }
    return 0;
}
