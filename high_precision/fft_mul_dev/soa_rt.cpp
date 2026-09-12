// 权威往返测试: DIT(DIF(x)) == x * n ; 并行比对 SoA vs AoS
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
        Fr[k] = _mm_cvtsd_f64(D[k]);
        Fi[k] = _mm_cvtsd_f64(_mm_unpackhi_pd(D[k], D[k]));
    }
}
static double maxabs(const double* a, const double* b, u32 n) {
    double m = 0; for (u32 k = 0; k != n; ++k) m = std::fmax(m, std::fabs(a[k]-b[k])); return m;
}

int main() {
    const u32 ns[] = {256, 512, 1024, 2048, 4096, 16384};
    std::mt19937_64 rng(98765);
    bool allok = true;
    for (u32 ni = 0; ni != 4; ++ni) {
        const u32 n = ns[ni];
        std::vector<cpx> x(n);
        for (u32 k = 0; k != n; ++k) {
            double r = (double)(rng() & 0xffff) / 65536.0 - 0.5;
            double i = (double)(rng() & 0xffff) / 65536.0 - 0.5;
            x[k] = mk(r, i);
        }
        // AoS: DIF then DIT
        fft_ref::resize(n);
        std::vector<cpx> A = x;
        fft_ref::difRec(A.data(), n, 0);
        fft_ref::ditRec(A.data(), n, 0);
        std::vector<double> A_r(n), A_i(n);
        for (u32 k = 0; k != n; ++k) { A_r[k] = _mm_cvtsd_f64(A[k]); A_i[k] = _mm_cvtsd_f64(_mm_unpackhi_pd(A[k], A[k])); }
        // SoA: DIF then DIT
        fft_soa::resize(n);
        std::vector<double> Fr(n), Fi(n);
        aos_to_soa(x, Fr.data(), Fi.data(), n);
        fft_soa::difRec(Fr.data(), Fi.data(), n, 0, 0);
        fft_soa::ditRec(Fr.data(), Fi.data(), n, 0, 0);
        // 期望 AoS/SoA == x * n (往返)
        std::vector<double> exp_r(n), exp_i(n);
        for (u32 k = 0; k != n; ++k) { exp_r[k] = (double)n * _mm_cvtsd_f64(x[k]); exp_i[k] = (double)n * _mm_cvtsd_f64(_mm_unpackhi_pd(x[k], x[k])); }
        double err_soa_vs_xn = std::fmax(maxabs(Fr.data(), exp_r.data(), n), maxabs(Fi.data(), exp_i.data(), n));
        double err_aos_vs_xn = std::fmax(maxabs(A_r.data(), exp_r.data(), n), maxabs(A_i.data(), exp_i.data(), n));
        double err_soa_vs_aos = std::fmax(maxabs(Fr.data(), A_r.data(), n), maxabs(Fi.data(), A_i.data(), n));
        printf("n=%u  SoA_vs_x*n=%.3e  AoS_vs_x*n=%.3e  SoA_vs_AoS=%.3e\n",
               n, err_soa_vs_xn, err_aos_vs_xn, err_soa_vs_aos);
        if (err_soa_vs_xn > 1e-6 || err_soa_vs_aos > 1e-6) allok = false;
    }
    printf(allok ? "=== ROUNDTRIP ALL PASS ===\n" : "=== ROUNDTRIP FAIL ===\n");
    return allok ? 0 : 1;
}
