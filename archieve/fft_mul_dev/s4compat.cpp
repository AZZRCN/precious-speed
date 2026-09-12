// s4compat.cpp v2 -- decisive drop-in test for 4-step.
// Sub-transforms use a CORRECT recursive radix-2 FFT (verified), so any failure is in the
// 4-step structure/twiddle, not the sub-transform.
// Test: does (fourstep_fwd + pointwise + fourstep_inv) reproduce correct circular convolution?
// pointwise assumes base-4-reversal output order; if fourstep produces that order, conv matches.
#include <cstdio>
#include <cmath>
#include <vector>
#include <complex>
#include <random>
#include <algorithm>

using cd = std::complex<double>;
static const double PI = 3.14159265358979323846;

// forward radix-2 DIT FFT, in-place, natural-in natural-out
static void fft_r2(std::vector<cd>& a, bool inv) {
    int n = (int)a.size();
    // bit-reversal permutation
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = (inv ? 2 : -2) * PI / len;
        cd wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            cd w(1, 0);
            for (int k = 0; k < len / 2; ++k) {
                cd u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inv) for (int i = 0; i < n; ++i) a[i] /= n;
}

// naive circular convolution (reference)
static void naive_conv(const std::vector<cd>& a, const std::vector<cd>& b, std::vector<cd>& c) {
    int n = (int)a.size();
    c.assign(n, cd(0,0));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            c[(i + j) % n] += a[i] * b[j];
}

// ---------- 4-step forward ----------
// N = N1 * N2. layout A[n2][n1] = d[n1 + n2*N1].
// variant selects twiddle placement tested.
enum Variant { PRE_N1N2, PRE_N1K2, POST_N2K1, PRE_AND_POST };
static void fourstep_fwd(std::vector<cd>& d, int N, int N1, int N2, Variant v) {
    // optional pre-twiddle W_N^{n1*n2}
    if (v == PRE_N1N2 || v == PRE_AND_POST) {
        for (int n2 = 0; n2 < N2; ++n2)
            for (int n1 = 0; n1 < N1; ++n1)
                d[n1 + n2*N1] *= cd(std::cos(-2*PI*n1*n2/N), std::sin(-2*PI*n1*n2/N));
    }
    if (v == PRE_N1K2) {
        // pre-twiddle W_N^{n1*n2} applied to columns (acts as W_N^{n1*k2} via column index)
        for (int n2 = 0; n2 < N2; ++n2)
            for (int n1 = 0; n1 < N1; ++n1)
                d[n1 + n2*N1] *= cd(std::cos(-2*PI*n1*n2/N), std::sin(-2*PI*n1*n2/N));
    }
    // DFT-N1 along n1 (rows)
    for (int n2 = 0; n2 < N2; ++n2) {
        std::vector<cd> row(d.begin() + n2*N1, d.begin() + n2*N1 + N1);
        fft_r2(row, false);
        for (int n1 = 0; n1 < N1; ++n1) d[n1 + n2*N1] = row[n1];
    }
    // transpose N1xN2 -> N2xN1
    std::vector<cd> t(N);
    for (int n2 = 0; n2 < N2; ++n2)
        for (int n1 = 0; n1 < N1; ++n1)
            t[n2 + n1*N2] = d[n1 + n2*N1];
    d = t;
    if (v == POST_N2K1) {
        for (int n1 = 0; n1 < N1; ++n1)
            for (int n2 = 0; n2 < N2; ++n2)
                d[n2 + n1*N2] *= cd(std::cos(-2*PI*n2*n1/N), std::sin(-2*PI*n2*n1/N));
    }
    // DFT-N2 along n2 (now rows of size N2)
    for (int n1 = 0; n1 < N1; ++n1) {
        std::vector<cd> row(d.begin() + n1*N2, d.begin() + n1*N2 + N2);
        fft_r2(row, false);
        for (int n2 = 0; n2 < N2; ++n2) d[n2 + n1*N2] = row[n2];
    }
    // transpose back
    std::vector<cd> t2(N);
    for (int n1 = 0; n1 < N1; ++n1)
        for (int n2 = 0; n2 < N2; ++n2)
            t2[n1 + n2*N1] = d[n2 + n1*N2];
    d = t2;
    if (v == PRE_AND_POST) {
        for (int k2 = 0; k2 < N2; ++k2)
            for (int k1 = 0; k1 < N1; ++k1)
                d[k1 + k2*N1] *= cd(std::cos(-2*PI*k2*k1/N), std::sin(-2*PI*k2*k1/N));
    }
}

// ---------- 4-step inverse (reverse order) ----------
static void fourstep_inv(std::vector<cd>& d, int N, int N1, int N2, Variant v) {
    // reverse of fwd: transpose, IDFT-N2, transpose, IDFT-N1, post/pre twiddle reversed
    if (v == PRE_AND_POST) {
        for (int k2 = 0; k2 < N2; ++k2)
            for (int k1 = 0; k1 < N1; ++k1)
                d[k1 + k2*N1] *= cd(std::cos(2*PI*k2*k1/N), std::sin(2*PI*k2*k1/N));
    }
    std::vector<cd> t(N);
    for (int n1 = 0; n1 < N1; ++n1)
        for (int n2 = 0; n2 < N2; ++n2)
            t[n2 + n1*N2] = d[n1 + n2*N1];
    d = t;
    for (int n1 = 0; n1 < N1; ++n1) {
        std::vector<cd> row(d.begin() + n1*N2, d.begin() + n1*N2 + N2);
        fft_r2(row, true);
        for (int n2 = 0; n2 < N2; ++n2) d[n2 + n1*N2] = row[n2];
    }
    std::vector<cd> t2(N);
    for (int n2 = 0; n2 < N2; ++n2)
        for (int n1 = 0; n1 < N1; ++n1)
            t2[n1 + n2*N1] = d[n2 + n1*N2];
    d = t2;
    if (v == POST_N2K1) {
        for (int n1 = 0; n1 < N1; ++n1)
            for (int n2 = 0; n2 < N2; ++n2)
                d[n1 + n2*N1] *= cd(std::cos(2*PI*n2*n1/N), std::sin(2*PI*n2*n1/N));
    }
    for (int n2 = 0; n2 < N2; ++n2) {
        std::vector<cd> row(d.begin() + n2*N1, d.begin() + n2*N1 + N1);
        fft_r2(row, true);
        for (int n1 = 0; n1 < N1; ++n1) d[n1 + n2*N1] = row[n1];
    }
    if (v == PRE_N1N2 || v == PRE_N1K2) {
        for (int n2 = 0; n2 < N2; ++n2)
            for (int n1 = 0; n1 < N1; ++n1)
                d[n1 + n2*N1] *= cd(std::cos(2*PI*n1*n2/N), std::sin(2*PI*n1*n2/N));
    }
}

// pointwise (scalar, mirrors existing algorithm's contract: assumes base-4 reversal order)
static void pointwise(std::vector<cd>& F, const std::vector<cd>& G, int n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    F[0] *= G[0] * nf;
    F[1] *= G[1] * nf;
    for (int bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        for (int f = bs, b = f + bs - 1; f != be; ++f, --b) {
            cd Fc = std::conj(F[b]), Gc = std::conj(G[b]);
            cd fe = F[f] + Fc, fo = F[f] - Fc;
            cd ge = G[f] + Gc, go = G[f] - Gc;
            cd tf = (f & 1) ? -std::conj(cd(std::cos(-2*PI*(f>>1)/n), std::sin(-2*PI*(f>>1)/n)))
                            : cd(std::cos(-2*PI*(f>>1)/n), std::sin(-2*PI*(f>>1)/n));
            cd pa = fe*ge - fo*go*tf;
            cd pb = ge*fo + fe*go;
            F[f] = (pa + pb) * sf;
            F[b] = std::conj((pa - pb) * sf);
        }
    }
}

static double maxdiff(const std::vector<cd>& a, const std::vector<cd>& b) {
    double m = 0;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

int main() {
    const int m = 4;
    const int N = 1 << (2*m);   // 256
    const int N1 = 1 << m, N2 = 1 << m;
    printf("N=%d N1=%d N2=%d\n", N, N1, N2);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> dist(-1, 1);

    const char* names[] = {"PRE_N1N2", "PRE_N1K2", "POST_N2K1", "PRE_AND_POST"};
    for (int vi = 0; vi < 4; ++vi) {
        Variant v = (Variant)vi;
        std::vector<cd> a(N), b(N);
        for (int i = 0; i < N; ++i) { a[i] = cd(dist(rng), dist(rng)); b[i] = cd(dist(rng), dist(rng)); }

        std::vector<cd> fa = a, fb = b;
        fourstep_fwd(fa, N, N1, N2, v);
        fourstep_fwd(fb, N, N1, N2, v);
        // round-trip check
        std::vector<cd> rt = fa;
        fourstep_inv(rt, N, N1, N2, v);
        // conv check
        pointwise(fa, fb, N);
        fourstep_inv(fa, N, N1, N2, v);
        // plain element-wise pointwise (self-contained, order-agnostic)
        std::vector<cd> fa2 = a, fb2 = b;
        fourstep_fwd(fa2, N, N1, N2, v);
        fourstep_fwd(fb2, N, N1, N2, v);
        for (int i = 0; i < N; ++i) fa2[i] *= fb2[i] * (1.0 / N);
        fourstep_inv(fa2, N, N1, N2, v);
        std::vector<cd> ref;
        naive_conv(a, b, ref);
        printf("[%s] roundtrip_max=%.2e  folded_conv_max=%.2e  plain_conv_max=%.2e\n",
               names[vi], maxdiff(rt, a), maxdiff(fa, ref), maxdiff(fa2, ref));
    }
    return 0;
}
