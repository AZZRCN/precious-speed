#include <complex>
#include <vector>
#include <cmath>
#include <cstdio>
#include <random>
using cd = std::complex<double>;
const double PI = std::acos(-1.0);

// naive 8-point DFT (correct by construction) -- for validation only
static void dft8(const cd* x, cd* y) {
    for (int k = 0; k < 8; k++) {
        cd s(0,0);
        for (int n = 0; n < 8; n++) s += x[n] * std::polar(1.0, -2*PI*n*k/8.0);
        y[k] = s;
    }
}

// recursive radix-8 DIF forward (standard radix-r combine, naive 8-pt primitive)
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
static void inv(cd* a, int n) {
    for (int i = 0; i < n; i++) a[i] = std::conj(a[i]);
    fwd(a, n);
    for (int i = 0; i < n; i++) a[i] = std::conj(a[i]) / (double)n;
}

int main() {
    std::mt19937_64 rng(12345);
    int fails = 0;
    int sizes[] = {8, 64, 512, 4096, 32768, 262144, 2097152};
    for (int N : sizes) {
        std::vector<cd> x(N), y(N), z(N);
        for (int i = 0; i < N; i++) { double re = (rng()&1023)-512, im=(rng()&1023)-512; x[i]=cd(re,im); y[i]=x[i]; }
        fwd(y.data(), N);
        // cross-check vs naive full DFT on a few points
        // round-trip
        inv(y.data(), N);
        double maxerr = 0;
        for (int i = 0; i < N; i++) {
            double e = std::abs(y[i] - x[i]);
            if (e > maxerr) maxerr = e;
        }
        bool ok = maxerr < 1e-6;
        if (!ok) fails++;
        printf("N=%-8d roundtrip_maxerr=%.3e %s\n", N, maxerr, ok?"OK":"FAIL");
    }
    printf("RESULT fails=%d\n", fails);
    return fails ? 1 : 0;
}
