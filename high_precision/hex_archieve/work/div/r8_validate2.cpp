#include <complex>
#include <vector>
#include <cmath>
#include <cstdio>
#include <random>
using cd = std::complex<double>;
const double PI = std::acos(-1.0);

static void dft8(const cd* x, cd* y) {
    for (int k = 0; k < 8; k++) {
        cd s(0,0);
        for (int n = 0; n < 8; n++) s += x[n] * std::polar(1.0, -2*PI*n*k/8.0);
        y[k] = s;
    }
}
static void fwd(cd* a, int n) {
    if (n == 1) return;
    if (n == 8) { cd t[8]; dft8(a, t); for (int i=0;i<8;i++) a[i]=t[i]; return; }
    int m = n / 8;
    for (int i = 0; i < 8; i++) fwd(a + i*m, m);
    std::vector<cd> tmp(8);
    for (int j = 0; j < m; j++) {
        for (int k = 0; k < 8; k++) tmp[k] = a[k*m + j] * std::polar(1.0, -2*PI*j*k/n);
        cd col[8]; dft8(tmp.data(), col);
        for (int k = 0; k < 8; k++) a[k*m + j] = col[k];
    }
}
static std::vector<cd> naive_dft(const std::vector<cd>& a) {
    int N = (int)a.size(); std::vector<cd> y(N);
    for (int k=0;k<N;k++) for (int n=0;n<N;n++) y[k]+=a[n]*std::polar(1.0,-2*PI*n*k/N);
    return y;
}
int main() {
    std::mt19937_64 rng(99);
    int sizes[] = {8, 64, 512, 4096};
    for (int N : sizes) {
        std::vector<cd> x(N), y(N);
        for (int i=0;i<N;i++){ double re=(rng()&1023)-512, im=(rng()&1023)-512; x[i]=cd(re,im); y[i]=x[i]; }
        fwd(y.data(), N);
        std::vector<cd> ref = naive_dft(x);
        double me=0; for(int i=0;i<N;i++){ double e=std::abs(y[i]-ref[i]); if(e>me) me=e; }
        printf("N=%-6d fwd_vs_naive_maxerr=%.3e %s\n", N, me, me<1e-6?"OK":"FAIL");
    }
    return 0;
}
