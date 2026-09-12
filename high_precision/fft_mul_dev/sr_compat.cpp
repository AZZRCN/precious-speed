// split-radix 正确性 harness：独立验证 (1) 往返误差 (2) 输出=naive DFT（自然序）
// 用 Wikipedia 标准 split-radix DIT（out-of-place），先锁数学，再谈生产集成。
#include <cstdio>
#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
using cd = std::complex<double>;
static const double PI = std::acos(-1.0);
static inline cd expi(double a){ return cd(std::cos(a), std::sin(a)); }

void naive_dft(const cd* x, cd* y, int n, int sign){
    for(int k=0;k<n;k++){
        cd s(0,0);
        for(int m=0;m<n;m++) s += x[m] * expi(sign * 2.0*PI*k*m/n);
        y[k]=s;
    }
}

// 标准 split-radix DIT：x->y，sign=-1 正向。自然序输入/输出。
void sr_fft(const cd* x, cd* y, int n, int sign){
    if(n==1){ y[0]=x[0]; return; }
    if(n==2){ y[0]=x[0]+x[1]; y[1]=x[0]-x[1]; return; }
    int q=n/4;
    std::vector<cd> even(n/2), odd1(q), odd3(q);
    for(int i=0;i<n/2;i++)   even[i]=x[2*i];
    for(int i=0;i<q;i++){ odd1[i]=x[4*i+1]; odd3[i]=x[4*i+3]; }
    std::vector<cd> G(n/2), H1(q), H3(q);
    sr_fft(even.data(), G.data(), n/2, sign);
    sr_fft(odd1.data(), H1.data(), q, sign);
    sr_fft(odd3.data(), H3.data(), q, sign);
    const cd I(0,1);
    for(int k=0;k<q;k++){
        cd w1 = H1[k] * expi(sign * 2.0*PI*k/n);          // W_N^k
        cd w3 = H3[k] * expi(sign * 2.0*PI*3*k/n);        // W_N^{3k}
        cd t = w1 + w3;
        cd u = w1 - w3;
        y[k]       = G[k]       + t;
        y[k+q]     = G[k+q]     - I*u;
        y[k+2*q]   = G[k]       - t;
        y[k+3*q]   = G[k+q]     + I*u;
    }
}

static double maxdiff(const cd* a, const cd* b, int n){
    double m=0; for(int i=0;i<n;i++) m=std::max(m, std::abs(a[i]-b[i])); return m;
}

int main(){
    printf("=== split-radix (Wikipedia DIT) 自检 vs naive DFT ===\n");
    for(int n : {4,8,16,32,64,128,256,1024}){
        std::vector<cd> x(n), f(n), ref(n), inv_ref(n);
        for(int i=0;i<n;i++) x[i]=cd(rand()%1000-500, rand()%1000-500)/100.0;
        naive_dft(x.data(), ref.data(), n, -1);
        naive_dft(x.data(), inv_ref.data(), n, +1);       // naive 逆
        sr_fft(x.data(), f.data(), n, -1);
        double df = maxdiff(f.data(), ref.data(), n);
        // 逆：标准共轭技巧  IDFT(y) = conj(DFT(conj(y)))/n
        std::vector<cd> fc(n), g(n);
        for(int i=0;i<n;i++) fc[i]=std::conj(f[i]);
        sr_fft(fc.data(), g.data(), n, -1);
        for(int i=0;i<n;i++) g[i]=std::conj(g[i])/cd(n);
        double rt = maxdiff(x.data(), g.data(), n);
        // sign=+1 是否直接等于 naive 逆？
        std::vector<cd> ff(n); sr_fft(x.data(), ff.data(), n, +1);
        double dpinv = maxdiff(ff.data(), inv_ref.data(), n);
        printf("[n=%4d] vs_naive_fwd=%.2e  rt(conj_trick)=%.2e  sign+1_vs_naive_inv=%.2e  %s\n",
               n, df, rt, dpinv, (df<1e-9 && rt<1e-9)?"OK":"FAIL");
    }
    return 0;
}
