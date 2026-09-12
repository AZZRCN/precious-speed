// 诊断: 对比 fft_soa vs fft_ref 的 difRec 与 ditRec (小 n)
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

static double run(const char* name, u32 n, bool inverse) {
    std::mt19937_64 rng(0x1234);
    auto rnd = [&](double s){ std::uniform_real_distribution<double> d(-s,s); return d(rng); };
    std::vector<double> A(2*n), Fr(n), Fi(n);
    for (u32 i=0;i<n;++i){ A[2*i]=rnd(1.0); A[2*i+1]=rnd(1.0); Fr[i]=A[2*i]; Fi[i]=A[2*i+1]; }
    std::vector<double> B(2*n); memcpy(B.data(), A.data(), 2*n*8);
    if (inverse) { fft_ref::resize(n); fft_ref::ditRec((cpx*)B.data(), n, 0); fft_soa::resize(n); fft_soa::ditRec(Fr.data(), Fi.data(), n, 0); }
    else          { fft_ref::resize(n); fft_ref::difRec((cpx*)B.data(), n, 0); fft_soa::resize(n); fft_soa::difRec(Fr.data(), Fi.data(), n, 0); }
    double mx=0;
    for(u32 i=0;i<n;++i){ mx=std::max(mx,std::fabs(Fr[i]-cre(((cpx*)B.data())[i]))); mx=std::max(mx,std::fabs(Fi[i]-cim(((cpx*)B.data())[i]))); }
    printf("=== %s n=%u  maxerr=%.4e ===\n", name, n, mx);
    for (u32 i=0;i<std::min(n,(u32)6);++i)
        printf("  [%2u] ref=(%.6f,%.6f) soa=(%.6f,%.6f)\n", i,
               cre(((cpx*)B.data())[i]), cim(((cpx*)B.data())[i]), Fr[i], Fi[i]);
    return mx;
}

int main() {
    for (u32 n : {256u, 512u, 1024u}) {
        run("difRec", n, false);
        run("ditRec", n, true);
    }
    return 0;
}
