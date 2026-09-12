// Lock 4-step STRUCTURE by brute force. Sub-transforms = naive DFT (correct).
// Enumerate: axis1, tp1(transpose after tw1), axis2, s1, s2, final indexing.
#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>
#include <cstdlib>
#include <algorithm>
using cx = std::complex<double>;
static const double PI = std::acos(-1.0);
static inline cx W(int n, long long k) { return std::exp(cx(0, -2.0*PI*k/n)); }
static void naive_dft(const cx* x, cx* y, int n) {
    for (int k=0;k<n;++k){ cx s(0,0); for(int m=0;m<n;++m) s+=x[m]*W(n,(long long)m*k); y[k]=s; }
}
// x stored x[n2*N1+n1]; helpers to read/write 2D
static void fourstep(const cx* x, int N, int N1, int N2, int axis1, int tp1, int axis2, int s1, int s2, std::vector<cx>& out) {
    auto get=[&](const std::vector<cx>& a, int n2, int n1)->cx{ return a[n2*N1+n1]; };
    auto set=[&](std::vector<cx>& a, int n2, int n1, cx v){ a[n2*N1+n1]=v; };
    std::vector<cx> t(N);
    for(int n2=0;n2<N2;++n2) for(int n1=0;n1<N1;++n1) t[n2*N1+n1]=x[n2*N1+n1]*W(N,(long long)s1*n1*n2);
    // step1: DFT over axis1 (0=n1 size N1, 1=n2 size N2)
    std::vector<cx> Y(N);
    if(axis1==0){ for(int n2=0;n2<N2;++n2){ std::vector<cx> r(N1),o(N1); for(int n1=0;n1<N1;++n1) r[n1]=t[n2*N1+n1]; naive_dft(r.data(),o.data(),N1); for(int n1=0;n1<N1;++n1) Y[n2*N1+n1]=o[n1]; } }
    else { for(int n1=0;n1<N1;++n1){ std::vector<cx> r(N2),o(N2); for(int n2=0;n2<N2;++n2) r[n2]=t[n2*N1+n1]; naive_dft(r.data(),o.data(),N2); for(int n2=0;n2<N2;++n2) Y[n2*N1+n1]=o[n2]; } }
    // optional transpose
    std::vector<cx> A = Y;
    if(tp1){ for(int n2=0;n2<N2;++n2) for(int n1=0;n1<N1;++n1) A[n1*N1+n2]=Y[n2*N1+n1]; }
    // twiddle2 (after transpose, applied to A's current layout): W_N^{s2 * i*j} over its 2D indices
    if(tp1){ for(int n2=0;n2<N2;++n2) for(int n1=0;n1<N1;++n1) A[n1*N1+n2]*=W(N,(long long)s2*n1*n2); }
    else   { for(int n2=0;n2<N2;++n2) for(int n1=0;n1<N1;++n1) A[n2*N1+n1]*=W(N,(long long)s2*n1*n2); }
    // step2: DFT over axis2 on A (A is N2xN1 if tp1 else N1xN2; treat as 2D with dims)
    std::vector<cx> Z(N);
    if(tp1){
        // A is N1(rows n2') x N2(cols n1') : A[n2'*N1? ] hmm A[n1*N1+n2] => rows n1 (0..N2-1)? N1 here is second dim. Let dims: rows=n2'(0..N2-1)? 
        // A[n1*N1+n2], n1 in [0,N2), n2 in [0,N1) -> it's N2 x N1.
        if(axis2==0){ // DFT over n1 (size N1) per row n2'
            for(int r=0;r<N2;++r){ std::vector<cx> rr(N1),oo(N1); for(int c=0;c<N1;++c) rr[c]=A[r*N1+c]; naive_dft(rr.data(),oo.data(),N1); for(int c=0;c<N1;++c) Z[r*N1+c]=oo[c]; }
        } else { for(int c=0;c<N1;++c){ std::vector<cx> rr(N2),oo(N2); for(int r=0;r<N2;++r) rr[r]=A[r*N1+c]; naive_dft(rr.data(),oo.data(),N2); for(int r=0;r<N2;++r) Z[r*N1+c]=oo[r]; } }
    } else {
        // A is N1 x N2 (A[n2*N1+n1])
        if(axis2==0){ for(int r=0;r<N1;++r){ std::vector<cx> rr(N2),oo(N2); for(int c=0;c<N2;++c) rr[c]=A[r*N1+c]; naive_dft(rr.data(),oo.data(),N2); for(int c=0;c<N2;++c) Z[r*N1+c]=oo[c]; } }
        else { for(int c=0;c<N2;++c){ std::vector<cx> rr(N1),oo(N1); for(int r=0;r<N1;++r) rr[r]=A[r*N1+c]; naive_dft(rr.data(),oo.data(),N1); for(int r=0;r<N1;++r) Z[r*N1+c]=oo[r]; } }
    }
    out = Z;
}
static int fail=0;
int main(){
    int Ns[]={16,256,4096};
    for(int ti=0;ti<3;++ti){
        int N=Ns[ti]; int N1=(int)std::sqrt((double)N); int N2=N1;
        std::vector<cx> x(N),ref(N); srand(N+11);
        for(int i=0;i<N;++i){double re=(double)rand()/RAND_MAX*2-1,im=(double)rand()/RAND_MAX*2-1;x[i]=cx(re,im);}
        naive_dft(x.data(),ref.data(),N);
        double best=1e30; int ba1=0,bt=0,ba2=0,bs1=0,bs2=0,bi=0;
        std::vector<cx> Z;
        for(int a1=0;a1<2;++a1) for(int tp=0;tp<2;++tp) for(int a2=0;a2<2;++a2) for(int s1=-1;s1<=1;s1+=2) for(int s2=-1;s2<=1;s2+=2) for(int id=0;id<4;++id){
            fourstep(x.data(),N,N1,N2,a1,tp,a2,s1,s2,Z);
            double e=0;
            // Z layout depends on tp/a2; just compare as a permutation by checking multiset equality to ref
            std::vector<cx> sz=Z, sr=ref;
            std::sort(sz.begin(),sz.end(),[](cx a,cx b){return std::abs(a)<std::abs(b);});
            std::sort(sr.begin(),sr.end(),[](cx a,cx b){return std::abs(a)<std::abs(b);});
            for(int i=0;i<N;++i) e=std::max(e,std::abs(sz[i]-sr[i]));
            if(e<best){best=e;ba1=a1;bt=tp;ba2=a2;bs1=s1;bs2=s2;bi=id;}
        }
        printf("N=%-6d best(multiset)=%.2e (a1=%d tp=%d a2=%d s1=%d s2=%d)\n",N,best,ba1,bt,ba2,bs1,bs2); fflush(stdout);
        if(best>1e-9) fail=1;
    }
    printf(fail?"FAIL structure\n":"PASS structure locked\n");
    return fail;
}
