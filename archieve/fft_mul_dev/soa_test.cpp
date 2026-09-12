// 比特级对照: fft_soa (SoA) vs fft_ref (AoS) 在随机输入上逐复数对比。
#include "prod_fft.h"
#include "soa_fft.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>
#include <cstring>

using cpx = __m128d;
static inline double cre(cpx z){ return _mm_cvtsd_f64(z); }
static inline double cim(cpx z){ return _mm_cvtsd_f64(_mm_unpackhi_pd(z,z)); }

static int g_fail = 0;
static std::mt19937_64 rng(0x9E3779B97F4A7C15ULL);
static double rnd(double s){ std::uniform_real_distribution<double> d(-s,s); return d(rng); }

static void check(const char* name, u32 n, const std::vector<double>& Fr, const std::vector<double>& Fi,
                  const double* B, double tol) {
    double mx = 0;
    for (u32 i = 0; i < n; ++i) {
        double er = Fr[i] - cre(((const cpx*)B)[i]);
        double ei = Fi[i] - cim(((const cpx*)B)[i]);
        mx = std::max(mx, std::fabs(er));
        mx = std::max(mx, std::fabs(ei));
    }
    printf("%-14s n=%-7u maxerr=%.3e %s\n", name, n, mx, mx <= tol ? "OK" : "FAIL");
    if (mx > tol) ++g_fail;
}

int main() {
    const u32 sizes[] = {256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
    for (u32 n : sizes) {
        // ---- difRec (random complex) ----
        {
            std::vector<double> A(2*n), Fr(n), Fi(n);
            for (u32 i = 0; i < n; ++i) { A[2*i] = rnd(1.0); A[2*i+1] = rnd(1.0); Fr[i] = A[2*i]; Fi[i] = A[2*i+1]; }
            std::vector<double> B(2*n); memcpy(B.data(), A.data(), 2*n*8);
            fft_ref::resize(n); fft_ref::difRec((cpx*)B.data(), n, 0);
            fft_soa::resize(n); fft_soa::difRec(Fr.data(), Fi.data(), n, 0);
            check("difRec", n, Fr, Fi, B.data(), 1e-9);
        }
        // ---- ditRec (random complex) ----
        {
            std::vector<double> A(2*n), Fr(n), Fi(n);
            for (u32 i = 0; i < n; ++i) { A[2*i] = rnd(1.0); A[2*i+1] = rnd(1.0); Fr[i] = A[2*i]; Fi[i] = A[2*i+1]; }
            std::vector<double> B(2*n); memcpy(B.data(), A.data(), 2*n*8);
            fft_ref::resize(n); fft_ref::ditRec((cpx*)B.data(), n, 0);
            fft_soa::resize(n); fft_soa::ditRec(Fr.data(), Fi.data(), n, 0);
            check("ditRec", n, Fr, Fi, B.data(), 1e-9);
        }
        // ---- difRecZeroHi (real signal, upper half zero) ----
        {
            std::vector<double> A(2*n), Fr(n), Fi(n);
            for (u32 i = 0; i < n/2; ++i) A[2*i] = rnd(1.0);   // upper half (>=n/2) stays 0
            for (u32 i = 0; i < n; ++i) { Fr[i] = A[2*i]; Fi[i] = 0; }
            std::vector<double> B(2*n); memcpy(B.data(), A.data(), 2*n*8);
            fft_ref::resize(n); fft_ref::difRecZeroHi((cpx*)B.data(), n);
            fft_soa::resize(n); fft_soa::difRecZeroHi(Fr.data(), Fi.data(), n);
            check("difRecZeroHi", n, Fr, Fi, B.data(), 1e-9);
        }
        // ---- pointwise (two random) ----
        {
            std::vector<double> F(2*n), G(2*n), Fr(n), Fi(n), Gr(n), Gi(n);
            for (u32 i = 0; i < n; ++i) {
                F[2*i]=rnd(1.0); F[2*i+1]=rnd(1.0); G[2*i]=rnd(1.0); G[2*i+1]=rnd(1.0);
                Fr[i]=F[2*i]; Fi[i]=F[2*i+1]; Gr[i]=G[2*i]; Gi[i]=G[2*i+1];
            }
            std::vector<double> B(2*n); memcpy(B.data(), F.data(), 2*n*8);
            fft_ref::resize(n); fft_ref::pointwise((cpx*)B.data(), (cpx*)G.data(), n);
            fft_soa::resize(n); fft_soa::pointwise(Fr.data(), Fi.data(), Gr.data(), Gi.data(), n);
            check("pointwise", n, Fr, Fi, B.data(), 1e-9);
        }
        // ---- pointwiseSq (random) ----
        {
            std::vector<double> F(2*n), Fr(n), Fi(n);
            for (u32 i = 0; i < n; ++i) { F[2*i]=rnd(1.0); F[2*i+1]=rnd(1.0); Fr[i]=F[2*i]; Fi[i]=F[2*i+1]; }
            std::vector<double> B(2*n); memcpy(B.data(), F.data(), 2*n*8);
            fft_ref::resize(n); fft_ref::pointwiseSq((cpx*)B.data(), n);
            fft_soa::resize(n); fft_soa::pointwiseSq(Fr.data(), Fi.data(), n);
            check("pointwiseSq", n, Fr, Fi, B.data(), 1e-9);
        }
    }
    printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail ? 1 : 0;
}
