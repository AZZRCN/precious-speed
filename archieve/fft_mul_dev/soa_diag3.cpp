// 诊断3: 隔离 bf2InvOne + 关键 twiddle 对比
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

int main() {
    fft_ref::resize(256); fft_soa::resize(256);
    // twiddle 对比
    auto pr = [&](u32 i){
        cpx t = fft_ref::twg(i);
        double sr,si; fft_soa::twg(i,sr,si);
        printf("  twg(%u): ref=(%.8f,%.8f) soa=(%.8f,%.8f)\n", i, cre(t), cim(t), sr, si);
    };
    printf("=== twiddle twg ===\n"); pr(0); pr(1); pr(2); pr(3); pr(4); pr(5); pr(8); pr(16);
    auto pl = [&](u32 i){
        cpx t = fft_ref::twlo(i, fft_ref::twhi(i));
        double sr,si; fft_soa::twlo(i, 1.0, 0.0, sr, si); // hi=1
        printf("  twlo(%u,hi=1): ref=(%.8f,%.8f) soa=(%.8f,%.8f)\n", i, cre(t), cim(t), sr, si);
    };
    printf("=== twiddle twlo (hi=1) ===\n"); pl(0); pl(1); pl(2); pl(3); pl(4);

    // bf2InvOne 直接 (q=1)
    std::mt19937_64 rng(0x99);
    auto rnd=[&](double s){std::uniform_real_distribution<double>d(-s,s);return d(rng);};
    {
        std::vector<double> A(8), Fr(4), Fi(4);
        for(int i=0;i<4;++i){ A[2*i]=rnd(1); A[2*i+1]=rnd(1); Fr[i]=A[2*i]; Fi[i]=A[2*i+1]; }
        std::vector<double> B(8); memcpy(B.data(),A.data(),8*8);
        cpx t1 = fft_ref::twg(1);
        fft_ref::bf2InvOne((cpx*)B.data(), 1, t1);
        double w1r,w1i; fft_soa::twg(1,w1r,w1i);
        fft_soa::bf2InvOne(Fr.data(),Fi.data(),0,1,w1r,w1i);
        printf("--- bf2InvOne direct (q=1) ---\n");
        for(int i=0;i<4;++i) printf("  [%d] ref=(%.6f,%.6f) soa=(%.6f,%.6f)\n",i,cre(((cpx*)B.data())[i]),cim(((cpx*)B.data())[i]),Fr[i],Fi[i]);
    }
    return 0;
}
