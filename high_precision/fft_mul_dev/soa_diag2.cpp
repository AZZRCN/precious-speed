// 诊断2: 隔离测试 bf2Inv 与 ditFlat (直接调用, 不经递归)
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
    std::mt19937_64 rng(0xABCD);
    auto rnd=[&](double s){std::uniform_real_distribution<double>d(-s,s);return d(rng);};

    // ---- 1) bf2Inv 直接: 4 元素 ----
    {
        std::vector<double> A(8), Fr(4), Fi(4);
        for(int i=0;i<4;++i){ A[2*i]=rnd(1); A[2*i+1]=rnd(1); Fr[i]=A[2*i]; Fi[i]=A[2*i+1]; }
        std::vector<double> B(8); memcpy(B.data(),A.data(),8*8);
        fft_ref::resize(256); fft_ref::bf2Inv((cpx*)B.data(), 1, fft_ref::twg(1), fft_ref::twg(2), fft_ref::mulI(fft_ref::twg(2)));
        fft_soa::resize(256);
        double w1r,w1i,w0r,w0i,w2r,w2i; fft_soa::twg(1,w0r,w0i); fft_soa::twg(2,w1r,w1i); fft_soa::mulI(w1r,w1i,w2r,w2i);
        fft_soa::bf2Inv(Fr.data(),Fi.data(),0,1,w0r,w0i,w1r,w1i,w2r,w2i);
        printf("--- bf2Inv direct (q=1) ---\n");
        for(int i=0;i<4;++i) printf("  [%d] ref=(%.5f,%.5f) soa=(%.5f,%.5f)\n",i,cre(((cpx*)B.data())[i]),cim(((cpx*)B.data())[i]),Fr[i],Fi[i]);
    }

    // ---- 2) ditFlat 直接: n=16 ----
    {
        u32 n=16;
        std::vector<double> A(2*n), Fr(n), Fi(n);
        for(u32 i=0;i<n;++i){ A[2*i]=rnd(1); A[2*i+1]=rnd(1); Fr[i]=A[2*i]; Fi[i]=A[2*i+1]; }
        std::vector<double> B(2*n); memcpy(B.data(),A.data(),2*n*8);
        fft_ref::resize(n); fft_ref::ditFlat((cpx*)B.data(), n, 0);
        fft_soa::resize(n); fft_soa::ditFlat(Fr.data(), Fi.data(), n, 0, 0);
        double mx=0; for(u32 i=0;i<n;++i){ mx=std::max(mx,std::fabs(Fr[i]-cre(((cpx*)B.data())[i]))); mx=std::max(mx,std::fabs(Fi[i]-cim(((cpx*)B.data())[i]))); }
        printf("--- ditFlat direct n=%u  maxerr=%.4e ---\n", n, mx);
        for(u32 i=0;i<8;++i) printf("  [%2u] ref=(%.5f,%.5f) soa=(%.5f,%.5f)\n",i,cre(((cpx*)B.data())[i]),cim(((cpx*)B.data())[i]),Fr[i],Fi[i]);
    }

    // ---- 3) ditFlat 直接: n=256 ----
    {
        u32 n=256;
        std::vector<double> A(2*n), Fr(n), Fi(n);
        for(u32 i=0;i<n;++i){ A[2*i]=rnd(1); A[2*i+1]=rnd(1); Fr[i]=A[2*i]; Fi[i]=A[2*i+1]; }
        std::vector<double> B(2*n); memcpy(B.data(),A.data(),2*n*8);
        fft_ref::resize(n); fft_ref::ditFlat((cpx*)B.data(), n, 0);
        fft_soa::resize(n); fft_soa::ditFlat(Fr.data(), Fi.data(), n, 0, 0);
        double mx=0; for(u32 i=0;i<n;++i){ mx=std::max(mx,std::fabs(Fr[i]-cre(((cpx*)B.data())[i]))); mx=std::max(mx,std::fabs(Fi[i]-cim(((cpx*)B.data())[i]))); }
        printf("--- ditFlat direct n=%u  maxerr=%.4e ---\n", n, mx);
        for(u32 i=0;i<8;++i) printf("  [%2u] ref=(%.5f,%.5f) soa=(%.5f,%.5f)\n",i,cre(((cpx*)B.data())[i]),cim(((cpx*)B.data())[i]),Fr[i],Fi[i]);
    }
    return 0;
}
