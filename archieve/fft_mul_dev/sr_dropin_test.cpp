// 决定性契约测试：split-radix 前向(自然序) -> 重排 base-4 反转序 -> 标量复刻 production pointwise -> 逆 -> 比对 naive 循环卷积
#include <complex>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
using cd = std::complex<double>;
static const double PI = 3.14159265358979323846;
static cd expi(double a){ return cd(std::cos(a), std::sin(a)); }

// ---- split-radix DIT (out-of-place, natural output, forward sign=-1) --- 已 sr_compat 锁对 1e-15
void sr_fft(const cd* x, cd* y, int n, int sign){
    if(n==1){ y[0]=x[0]; return; }
    if(n==2){ y[0]=x[0]+x[1]; y[1]=x[0]-x[1]; return; }
    int q=n/2, h=n/4;
    std::vector<cd> G(q), H1(h), H3(h);
    for(int k=0;k<q;k++) G[k]=x[2*k];
    for(int k=0;k<h;k++){ H1[k]=x[4*k+1]; H3[k]=x[4*k+3]; }
    std::vector<cd> Gf(q), H1f(h), H3f(h);
    sr_fft(G.data(), Gf.data(), q, sign);
    sr_fft(H1.data(), H1f.data(), h, sign);
    sr_fft(H3.data(), H3f.data(), h, sign);
    for(int k=0;k<h;k++){
        cd w1 = H1f[k]*expi(sign*2.0*PI*k/n);
        cd w3 = H3f[k]*expi(sign*2.0*PI*3*k/n);
        cd t = w1 + w3;
        cd u = w1 - w3;
        y[k]     = Gf[k]   + t;
        y[k+h]   = Gf[k+h] + cd(0,1)*u;
        y[k+2*h] = Gf[k]   - t;
        y[k+3*h] = Gf[k+h] - cd(0,1)*u;
    }
}

// ---- 二进制位反转 (生产 difRec/ditRec 的实际输出序，非 base-4) ----
static int bitrev(int x, int bits){
    int r=0;
    for(int s=0;s<bits;s++){ int b=(x>>s)&1; r|= b<<(bits-1-s); }
    return r;
}

// ---- 标量 pointwise：复刻 production L541-583 算术 (twg = exp(-2pi i k/n), forward) ----
static cd twg(int k, int n){ return expi(-2.0*PI*k/n); }
static cd cmul(cd a, cd b){ return a*b; }
static cd conj(cd a){ return std::conj(a); }
static cd cscale(cd a, double s){ return a*s; }
static void pointwise_scalar(cd* F, cd* G, int n){
    const double nf = 1.0/n, sf = nf*0.25;
    F[0] = cscale(cmul(F[0],G[0]), nf);
    F[1] = cscale(cmul(F[1],G[1]), nf);
    for(int bs=2, be=3; bs!=n; bs<<=1, be<<=1){
        int f=bs, b=f+bs-1;
        for(; f!=be; ++f, --b){
            cd Fc = conj(F[b]), Gc = conj(G[b]);
            cd fe = F[f]+Fc, fo = F[f]-Fc;
            cd ge = G[f]+Gc, go = G[f]-Gc;
            cd t = (f&1) ? -conj(twg(f>>1,n)) : twg(f>>1,n);
            cd pa = cmul(fe,ge) - cmul(cmul(fo,go), t);
            cd pb = cmul(ge,fo) + cmul(fe,go);
            F[f] = cscale(pa+pb, sf);
            F[b] = conj(cscale(pa-pb, sf));
        }
    }
}

static double maxdiff(const cd* a, const cd* b, int n){
    double m=0; for(int i=0;i<n;i++){ double d=std::abs(a[i]-b[i]); if(d>m)m=d; } return m;
}

int main(){
    std::mt19937 rng(12345);
    printf("=== split-radix 前向 + 现有 pointwise 合同测试 (base-4 反转序对接) ===\n");
    bool allok=true;
    for(int bits : {4,6,8,10,12,14}){
        int n = 1<<bits;
        std::vector<cd> a(n), b(n);
        for(int i=0;i<n;i++){ a[i]=cd(rng()%1000-500, rng()%1000-500)/100.0;
                              b[i]=cd(rng()%1000-500, rng()%1000-500)/100.0; }
        // 前向：split-radix 自然序
        std::vector<cd> A(n), B(n);
        sr_fft(a.data(), A.data(), n, -1);
        sr_fft(b.data(), B.data(), n, -1);
        // 重排到 base-4 反转序 (F[bitrev(j)] = A[j])
        std::vector<cd> Ab4(n), Bb4(n);
        for(int j=0;j<n;j++){ int r=bitrev(j,bits); Ab4[r]=A[j]; Bb4[r]=B[j]; }
        // pointwise (production 算术)
        pointwise_scalar(Ab4.data(), Bb4.data(), n);
        // 逆：Ab4(base-4 rev) -> 自然序 -> 共轭技巧 ifft
        std::vector<cd> Anat2(n);
        for(int j=0;j<n;j++){ int r=bitrev(j,bits); Anat2[j]=Ab4[r]; }
        std::vector<cd> conj_in(n), conj_out(n);
        for(int j=0;j<n;j++) conj_in[j]=conj(Anat2[j]);
        sr_fft(conj_in.data(), conj_out.data(), n, -1);
        std::vector<cd> C(n);
        for(int j=0;j<n;j++) C[j]=conj(conj_out[j])/double(n);
        // naive 循环卷积
        std::vector<cd> ref(n, cd(0,0));
        for(int i=0;i<n;i++) for(int j=0;j<n;j++) ref[(i+j)%n]+=a[i]*b[j];
        double err=maxdiff(C.data(), ref.data(), n);
        bool ok = err<1e-6;
        if(!ok) allok=false;
        printf("[bits=%2d n=%6d] conv_err=%.2e  %s\n", bits, n, err, ok?"OK":"FAIL");
    }
    printf(allok?"ALL_OK\n":"SOME_FAIL\n");
    return 0;
}
