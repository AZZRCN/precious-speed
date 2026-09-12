// 诊断: AoS fft_ref::ditFlat(n,bb) vs SoA fft_soa::ditFlat(n,bb) 逐复数对照
#include "prod_fft.h"
#include "soa_fft.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>
#include <cstring>

using cpx = __m128d;
static cpx mk(double r, double i) { return _mm_set_pd(i, r); }
static void aos_to_soa(const std::vector<cpx>& D, double* Fr, double* Fi, u32 n) {
    for (u32 k = 0; k != n; ++k) {
        double r = _mm_cvtsd_f64(D[k]);
        double i = _mm_cvtsd_f64(_mm_unpackhi_pd(D[k], D[k]));
        Fr[k] = r; Fi[k] = i;
    }
}
static double maxdiff(const std::vector<cpx>& D, const double* Fr, const double* Fi, u32 n) {
    double m = 0;
    for (u32 k = 0; k != n; ++k) {
        double r = _mm_cvtsd_f64(D[k]);
        double i = _mm_cvtsd_f64(_mm_unpackhi_pd(D[k], D[k]));
        m = std::fmax(m, std::fabs(r - Fr[k]));
        m = std::fmax(m, std::fabs(i - Fi[k]));
    }
    return m;
}

int main() {
    const u32 ns[] = {64, 128, 256, 512};
    std::mt19937_64 rng(12345);
    for (u32 ni = 0; ni != 4; ++ni) {
        const u32 n = ns[ni];
        std::vector<cpx> in(n);
        for (u32 k = 0; k != n; ++k) {
            double r = (double)(rng() & 0xfff) / 4096.0 - 0.5;
            double i = (double)(rng() & 0xfff) / 4096.0 - 0.5;
            in[k] = mk(r, i);
        }
        for (u32 bb = 0; bb != 2; ++bb) {
            const u32 RN = (n < 512) ? 512 : n;   // 模拟真实管线: 用全尺寸 resize
            fft_ref::resize(RN);
            std::vector<cpx> Daos = in;
            fft_ref::ditFlat(Daos.data(), n, bb);
            fft_soa::resize(RN);
            std::vector<double> Fr(n), Fi(n);
            aos_to_soa(in, Fr.data(), Fi.data(), n);
            fft_soa::ditFlat(Fr.data(), Fi.data(), n, bb, 0);
            double md = maxdiff(Daos, Fr.data(), Fi.data(), n);
            const char* tail = (n == 64 || n == 256) ? "radix4-only" : "radix4+tail";
            printf("ditFlat n=%u bb=%u [%s]  maxdiff=%.3e\n", n, bb, tail, md);
            if (md > 1e-6) {
                for (u32 k = 0; k != n; ++k) {
                    double r = _mm_cvtsd_f64(Daos[k]);
                    double i = _mm_cvtsd_f64(_mm_unpackhi_pd(Daos[k], Daos[k]));
                    if (std::fabs(r - Fr[k]) + std::fabs(i - Fi[k]) > 1e-6) {
                        printf("  first diff at k=%u  aos=(%.6f,%.6f) soa=(%.6f,%.6f)\n",
                               k, r, i, Fr[k], Fi[k]);
                        break;
                    }
                }
            }
        }
    }
    return 0;
}
