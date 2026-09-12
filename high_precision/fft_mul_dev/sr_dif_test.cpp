// 验证 in-place split-radix DIF 是否可作为生产 difRec 的 drop-in：
//  (1) 往返（正向 DIF -> 共轭技巧逆）误差 ~1e-15
//  (2) dif_sr + 普通 pointwise(逐点乘) + dit_sr = 正确循环卷积 (vs naive)
// 若 (2) 通过 => FFT 内核可替换；剩余问题是输出排列是否 == radix-4 反转序(决定 folded pointwise 能否复用)。
#include <cstdio>
#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
using cd = std::complex<double>;
static const double PI = std::acos(-1.0);
static inline cd expi(double a){ return cd(std::cos(a), std::sin(a)); }

// in-place split-radix DIF (decimation in frequency). sign=-1 正向。
void sr_dif(cd* a, int n, int s){
    if(n<=1) return;
    if(n==2){ cd u=a[0], v=a[1]; a[0]=u+v; a[1]=(u-v); return; }
    int half=n/2, q=n/4;
    for(int i=0;i<half;i++){
        cd u=a[i], v=a[i+half];
        a[i]=u+v;
        a[i+half]=(u-v)*expi(s*2.0*PI*i/n);
    }
    std::vector<cd> e(q), o(q);
    for(int k=0;k<q;k++){ e[k]=a[2*k]; o[k]=a[2*k+1]; }
    sr_dif(e.data(), q, s);
    sr_dif(o.data(), q, s);
    for(int k=0;k<q;k++) o[k]*=expi(s*2.0*PI*(2*k+1)/n);
    for(int k=0;k<q;k++){ a[2*k]=e[k]; a[2*k+1]=o[k]; }
    sr_dif(a+half, half, s);
}
// in-place split-radix DIT (逆)。用共轭技巧在调用处处理；这里给一个直接 DIT 版本用于卷积测试。
void sr_dit(cd* a, int n, int s){
    if(n<=1) return;
    if(n==2){ cd u=a[0], v=a[1]; a[0]=u+v; a[1]=(u-v); return; }
    int half=n/2, q=n/4;
    sr_dit(a, half, s);   // 先递归第一半
    std::vector<cd> e(q), o(q);
    for(int k=0;k<q;k++){ e[k]=a[2*k]; o[k]=a[2*k+1]; }
    sr_dit(e.data(), q, s);
    sr_dit(o.data(), q, s);
    for(int k=0;k<q;k++) o[k]*=expi(s*2.0*PI*(2*k+1)/n);
    for(int k=0;k<q;k++){ a[2*k]=e[k]; a[2*k+1]=o[k]; }
    for(int i=0;i<half;i++){
        cd u=a[i], v=a[i+half];
        a[i]=u+v;
        a[i+half]=(u-v)*expi(s*2.0*PI*i/n);
    }
}

void naive_dft(const cd* x, cd* y, int n, int sign){
    for(int k=0;k<n;k++){ cd s(0,0); for(int m=0;m<n;m++) s+=x[m]*expi(sign*2.0*PI*k*m/n); y[k]=s; }
}
// 循环卷积 naive: c = a ⊗ b (长度 n)
void naive_conv(const std::vector<cd>& a, const std::vector<cd>& b, std::vector<cd>& c){
    int n=(int)a.size(); c.assign(n,cd(0,0));
    for(int i=0;i<n;i++) for(int j=0;j<n;j++) c[(i+j)%n]+=a[i]*b[j];
}
static double maxdiff(const cd* a, const cd* b, int n){
    double m=0; for(int i=0;i<n;i++) m=std::max(m,std::abs(a[i]-b[i])); return m;
}

int main(){
    printf("=== in-place split-radix DIF drop-in 可行性 ===\n");
    for(int n : {8,16,32,64,128,256,1024}){
        std::vector<cd> x(n), f(n);
        for(int i=0;i<n;i++) {} // noop
        for(int i=0;i<n;i++) x[i]=cd(rand()%1000-500,rand()%1000-500)/100.0;
        // (1) 往返: DIF 正向 -> DIT 逆向(共轭技巧)
        f=x; sr_dif(f.data(), n, -1);
        std::vector<cd> fc(n), g(n);
        for(int i=0;i<n;i++) fc[i]=std::conj(f[i]);
        sr_dit(fc.data(), n, -1);
        for(int i=0;i<n;i++) g[i]=std::conj(fc[i])/cd(n);
        double rt=maxdiff(x.data(), g.data(), n);
        // (2) 循环卷积: F=a^, G=b^, 逐点乘, 逆
        std::vector<cd> a=x, b(n), F(n), G(n), FG(n), conv(n), ref(n);
        for(int i=0;i<n;i++) b[i]=cd(rand()%1000-500,rand()%1000-500)/100.0;
        sr_dif(a.data(), n, -1); sr_dif(b.data(), n, -1);
        for(int i=0;i<n;i++) FG[i]=a[i]*b[i];
        std::vector<cd>FGc(n); for(int i=0;i<n;i++) FGc[i]=std::conj(FG[i]);
        sr_dit(FGc.data(), n, -1);
        for(int i=0;i<n;i++) conv[i]=std::conj(FGc[i])/cd(n);
        naive_conv(x, b, ref);
        double cc=maxdiff(conv.data(), ref.data(), n);
        printf("[n=%4d] roundtrip=%.2e  conv_vs_naive=%.2e  %s\n", n, rt, cc,
               (rt<1e-9 && cc<1e-9)?"OK":"FAIL");
    }
    return 0;
}
