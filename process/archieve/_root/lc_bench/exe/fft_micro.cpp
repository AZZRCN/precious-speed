// fft_micro.cpp - in-process microbenchmark: fused radix-4 vs radix-2 DIF/DIT.
//
// Builds by textually splicing the AVX2 TransformHelper out of mul_r4.cpp
// (see gen_fft_micro.sh). Measures pure transform time at every transform
// length the MUL path can hit, with no process-spawn / IO noise, and checks
// that both kernels agree to machine precision at each size.
#include <immintrin.h>
#include <complex>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>

#define VALIDITY_CHECK(cond, ex, msg)

namespace detail {
    constexpr std::uint32_t log2(std::uint32_t n) { return std::__lg(n); }
}

#include "transform_helper.inc"

static detail_TransformHelper T;

int main() {
    std::mt19937_64 rng(998244353);
    printf("%-10s %6s %12s %12s %9s   %s\n", "N", "log2", "radix2(ms)", "radix4(ms)", "speedup", "maxdiff");
    printf("%s\n", std::string(72, '-').c_str());

    for (std::uint32_t m = 10; m <= 22; ++m) {
        const std::uint32_t n = 1u << m;
        T.resize(n);
        std::vector<__m128d, /*aligned*/ std::allocator<__m128d>> ref(n), a(n), b(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            double lo = double(rng() % 10000), hi = double(rng() % 10000);
            ref[i] = _mm_set_pd(hi, lo);
        }
        // correctness: radix-2 vs radix-4 round of DIF then DIT
        std::memcpy(a.data(), ref.data(), n * sizeof(__m128d));
        std::memcpy(b.data(), ref.data(), n * sizeof(__m128d));
        T.decimationInFrequencyRadix2(a.data(), n);
        T.decimationInFrequency(b.data(), n);
        double maxdiff = 0;
        for (std::uint32_t i = 0; i < n; ++i) {
            double d0 = ((double*)&a[i])[0] - ((double*)&b[i])[0];
            double d1 = ((double*)&a[i])[1] - ((double*)&b[i])[1];
            maxdiff = std::max(maxdiff, std::max(std::abs(d0), std::abs(d1)));
        }
        T.decimationInTimeRadix2(a.data(), n);
        T.decimationInTime(b.data(), n);
        for (std::uint32_t i = 0; i < n; ++i) {
            double d0 = ((double*)&a[i])[0] - ((double*)&b[i])[0];
            double d1 = ((double*)&a[i])[1] - ((double*)&b[i])[1];
            maxdiff = std::max(maxdiff, std::max(std::abs(d0), std::abs(d1)));
        }

        const int reps = std::max(3u, (1u << 24) / n);
        auto bench = [&](int which) {
            // warm
            std::memcpy(a.data(), ref.data(), n * sizeof(__m128d));
            if (which == 2) { T.decimationInFrequencyRadix2(a.data(), n); T.decimationInTimeRadix2(a.data(), n); }
            else { T.decimationInFrequency(a.data(), n); T.decimationInTime(a.data(), n); }
            double best = 1e300;
            for (int t = 0; t < 5; ++t) {
                std::memcpy(a.data(), ref.data(), n * sizeof(__m128d));
                auto t0 = std::chrono::steady_clock::now();
                for (int r = 0; r < reps; ++r) {
                    if (which == 2) { T.decimationInFrequencyRadix2(a.data(), n); T.decimationInTimeRadix2(a.data(), n); }
                    else { T.decimationInFrequency(a.data(), n); T.decimationInTime(a.data(), n); }
                }
                auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count() / reps);
            }
            return best;
        };
        double t2 = bench(2), t4 = bench(4);
        printf("%-10u %6u %12.4f %12.4f %8.2fx   %.3e%s\n", n, m, t2, t4, t2 / t4, maxdiff,
               (m & 1) ? "  (odd)" : "");
    }

    // ---- precision margin -------------------------------------------------
    // mul.cpp rounds with int64(x + 0.5); a result is only safe while the
    // distance from the exact integer stays well under 0.5. Worst case for a
    // base-10^4 packed convolution is every digit = 9999.
    printf("\n%-10s %6s %14s %14s %10s\n", "N", "log2", "r2 maxErr", "r4 maxErr", "budget");
    printf("%s\n", std::string(60, '-').c_str());
    for (std::uint32_t m = 17; m <= 20; ++m) {
        const std::uint32_t n = 1u << m;
        T.resize(n);
        const std::uint32_t used = n / 2;   // operands fill half, rest zero-padded
        std::vector<__m128d> f0(n), s0(n), f(n), s(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            bool live = i < used;
            f0[i] = _mm_set_pd(live ? 9999.0 : 0.0, live ? 9999.0 : 0.0);
            s0[i] = f0[i];
        }
        double err[2];
        for (int which = 0; which < 2; ++which) {
            std::memcpy(f.data(), f0.data(), n * sizeof(__m128d));
            std::memcpy(s.data(), s0.data(), n * sizeof(__m128d));
            if (which == 0) {
                T.decimationInFrequencyRadix2(f.data(), n);
                T.decimationInFrequencyRadix2(s.data(), n);
                T.frequencyDomainPointwiseMultiply(f.data(), s.data(), n);
                T.decimationInTimeRadix2(f.data(), n);
            } else {
                T.decimationInFrequency(f.data(), n);
                T.decimationInFrequency(s.data(), n);
                T.frequencyDomainPointwiseMultiply(f.data(), s.data(), n);
                T.decimationInTime(f.data(), n);
            }
            double e = 0;
            for (std::uint32_t i = 0; i < n; ++i) {
                const double* p = (const double*)&f[i];
                e = std::max(e, std::abs(p[0] - std::nearbyint(p[0])));
                e = std::max(e, std::abs(p[1] - std::nearbyint(p[1])));
            }
            err[which] = e;
        }
        printf("%-10u %6u %14.3e %14.3e %9.1f%%\n", n, m, err[0], err[1],
               err[1] / 0.5 * 100.0);
    }
    return 0;
}
