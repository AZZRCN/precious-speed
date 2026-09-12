// radix-3 FFT 档位自检: 以 2 幂 real_conv 为黄金参考, 校验 3*2^k 路径
#define main hint_div_main_disabled
#include "div_D12.cpp"
#undef main

#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#include <cmath>

using hint::fft_ceil;
using hint::int_ceil2;
using hint::is_fft3;

static int run_case(size_t n1, size_t n2, uint64_t seed, bool square)
{
    if (square)
        n2 = n1;
    const size_t conv_len = n1 + n2 - 1;
    const size_t FL2 = int_ceil2(conv_len);
    const size_t FL3 = fft_ceil(conv_len);
    if (!is_fft3(FL3))
        return -1; // 该长度没有落进 3*2^k 档位, 跳过

    std::mt19937_64 rng(seed);
    std::vector<double> a(n1), b(n2);
    for (auto &x : a)
        x = double(rng() % 10000);
    for (auto &x : b)
        x = double(rng() % 10000);

    hint::AlignedVec32<double> p1(FL2, 0.0), p2(FL2, 0.0);
    std::copy(a.begin(), a.end(), p1.begin());
    std::copy(b.begin(), b.end(), p2.begin());
    hint::transform::fft::real_conv(p1.data(), square ? p1.data() : p2.data(), FL2);

    hint::AlignedVec32<double> q1(FL3, 0.0), q2(FL3, 0.0);
    std::copy(a.begin(), a.end(), q1.begin());
    std::copy(b.begin(), b.end(), q2.begin());
    hint::transform::fft::real_conv(q1.data(), square ? q1.data() : q2.data(), FL3);

    double worst2 = 0, worst3 = 0, maxdiff = 0;
    size_t bad = 0;
    for (size_t i = 0; i < conv_len; i++)
    {
        double r2 = p1[i], r3 = q1[i];
        double i2 = std::round(r2), i3 = std::round(r3);
        worst2 = std::max(worst2, std::fabs(r2 - i2));
        worst3 = std::max(worst3, std::fabs(r3 - i3));
        maxdiff = std::max(maxdiff, std::fabs(r2 - r3));
        if (i2 != i3)
            bad++;
    }
    // 3*2^k 路径的尾部 [conv_len, FL3) 必须是 0
    double tail = 0;
    for (size_t i = conv_len; i < FL3; i++)
        tail = std::max(tail, std::fabs(q1[i]));

    printf("%s n1=%-7zu n2=%-7zu conv=%-7zu FL2=%-7zu FL3=%-7zu  err2=%.3e err3=%.3e diff=%.3e tail=%.3e  mismatch=%zu\n",
           bad == 0 && worst3 < 0.25 ? "[OK]  " : "[FAIL]",
           n1, n2, conv_len, FL2, FL3, worst2, worst3, maxdiff, tail, bad);
    return (bad == 0 && worst3 < 0.25) ? 0 : 1;
}

// ---------------- 阶段隔离诊断 ----------------
static size_t brev(size_t x, int bits)
{
    size_t r = 0;
    for (int i = 0; i < bits; i++)
        r = (r << 1) | ((x >> i) & 1);
    return r;
}

// A) 正变换 rdif 与朴素 DFT 逐点对拍 (只对小尺寸)
static int check_forward(size_t FL)
{
    const size_t M = FL / 6, blk = M * 2, N = FL / 2; // N = 3M complex
    int lg = 0;
    while ((size_t(1) << lg) < M)
        lg++;
    std::mt19937_64 rng(20260803 + FL);
    hint::AlignedVec32<double> p(FL);
    std::vector<double> x(FL);
    for (size_t i = 0; i < FL; i++)
        p[i] = x[i] = double(int(rng() % 2001) - 1000);

    hint::transform::fft::rdif(p.data(), FL);

    // 朴素 DFT: z[n] = x[2n] + i*x[2n+1], X[k] = sum z[n] e^{-2pi i nk/N}
    std::vector<long double> Xr(N), Xi(N);
    for (size_t k = 0; k < N; k++)
    {
        long double sr = 0, si = 0;
        for (size_t n = 0; n < N; n++)
        {
            long double th = -2.0L * 3.14159265358979323846264338L * (long double)((n * k) % N) / (long double)N;
            long double c = cosl(th), s = sinl(th);
            long double zr = x[2 * n], zi = x[2 * n + 1];
            sr += zr * c - zi * s;
            si += zr * s + zi * c;
        }
        Xr[k] = sr, Xi[k] = si;
    }
    double worst = 0;
    size_t badj = 99, badp = 0;
    for (size_t j = 0; j < 3; j++)
    {
        for (size_t pp = 0; pp < M; pp++)
        {
            size_t off = j * blk + (pp / 2) * 4 + (pp % 2);
            double gr = p[off], gi = p[off + 2];
            size_t f = 3 * brev(pp, lg) + j;
            double e = std::max(std::fabs(gr - (double)Xr[f]), std::fabs(gi - (double)Xi[f]));
            if (e > worst)
                worst = e, badj = j, badp = pp;
        }
    }
    double scale = 1000.0 * std::sqrt((double)N);
    bool ok = worst < scale * 1e-9;
    printf("%s FWD  FL=%-6zu M=%-5zu  maxerr=%.3e  (worst blk=%zu pos=%zu)\n",
           ok ? "[OK]  " : "[FAIL]", FL, M, worst, badj, badp);
    return ok ? 0 : 1;
}

// B) 往返 rdif -> ridit, 期望 out = (FL/2) * in
static int check_roundtrip(size_t FL)
{
    std::mt19937_64 rng(777 + FL);
    hint::AlignedVec32<double> p(FL);
    std::vector<double> x(FL);
    for (size_t i = 0; i < FL; i++)
        p[i] = x[i] = double(int(rng() % 2001) - 1000);
    hint::transform::fft::rdif(p.data(), FL);
    hint::transform::fft::ridit(p.data(), FL);
    const double S = double(FL / 2);
    double worst = 0, ratio = 0;
    for (size_t i = 0; i < FL; i++)
    {
        worst = std::max(worst, std::fabs(p[i] / S - x[i]));
        if (std::fabs(x[i]) > 1 && ratio == 0)
            ratio = p[i] / x[i];
    }
    bool ok = worst < 1e-6;
    printf("%s RT   FL=%-6zu  maxerr(/%.0f)=%.3e  obs_ratio=%.6f\n",
           ok ? "[OK]  " : "[FAIL]", FL, S, worst, ratio);
    return ok ? 0 : 1;
}

int main()
{
    int fails = 0, ran = 0;
    printf("---- stage A: forward vs naive DFT ----\n");
    for (size_t FL : {size_t(192), size_t(384), size_t(768)})
    {
        fails += check_forward(FL);
        ran++;
    }
    printf("---- stage B: round trip ----\n");
    for (size_t FL : {size_t(192), size_t(384), size_t(1536), size_t(49152)})
    {
        fails += check_roundtrip(FL);
        ran++;
    }
    printf("---- stage C: convolution vs 2-pow golden ----\n");
    // 覆盖各个量级, 都挑 "刚过 2 幂" 的长度以确保落进 3*2^k 档
    const size_t lens[] = {
        70, 100, 140, 200, 300, 400, 600, 800, 1100, 1500, 2100, 3000,
        4100, 6000, 8300, 12000, 17000, 25000, 34000, 50000, 68000,
        83334, 100000, 131073, 166670, 200000, 262145, 353013, 393000};
    for (size_t L : lens)
    {
        for (int mode = 0; mode < 3; mode++)
        {
            size_t n1 = L, n2 = L;
            if (mode == 1)
                n2 = L / 3 + 1;
            int r;
            if (mode == 2)
                r = run_case(L, L, 12345 + L, true);
            else
                r = run_case(n1, n2, 999 + L * 7 + mode, false);
            if (r >= 0)
            {
                ran++;
                fails += r;
            }
        }
    }
    // 极端数据: 全 9999 (最大动态范围)
    for (size_t L : {83334u, 166670u, 353013u})
    {
        size_t conv_len = 2 * L - 1;
        size_t FL3 = fft_ceil(conv_len), FL2 = int_ceil2(conv_len);
        if (!is_fft3(FL3))
            continue;
        hint::AlignedVec32<double> p(FL2, 0.0), q(FL3, 0.0);
        for (size_t i = 0; i < L; i++)
            p[i] = q[i] = 9999.0;
        hint::transform::fft::real_conv(p.data(), p.data(), FL2);
        hint::transform::fft::real_conv(q.data(), q.data(), FL3);
        double w2 = 0, w3 = 0;
        size_t bad = 0;
        for (size_t i = 0; i < conv_len; i++)
        {
            w2 = std::max(w2, std::fabs(p[i] - std::round(p[i])));
            w3 = std::max(w3, std::fabs(q[i] - std::round(q[i])));
            if (std::round(p[i]) != std::round(q[i]))
                bad++;
        }
        printf("%s ALL-9999 L=%-7zu conv=%-7zu FL2=%-7zu FL3=%-7zu err2=%.3e err3=%.3e mismatch=%zu\n",
               bad == 0 && w3 < 0.25 ? "[OK]  " : "[FAIL]", L, conv_len, FL2, FL3, w2, w3, bad);
        ran++;
        fails += (bad == 0 && w3 < 0.25) ? 0 : 1;
    }
    printf("\n==== ran=%d  fails=%d ====\n", ran, fails);
    return fails != 0;
}
