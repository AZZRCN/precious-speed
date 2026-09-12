// proto_split.cpp — Standard vs Split-coefficient FFT convolution benchmark.
//
// Question: does splitting each limb (0..9999, BASE=10^4) into hi (x/100, 0..99)
// and lo (x%100, 0..99) and doing 4 DFT + 1 IDFT beat the standard 2 DFT + 1 IDFT
// in precision, at the cost of more transforms? The survey (SURVEY.md B3/#9)
// judged "coefficients already small enough, no need to split" — this prototype
// verifies that claim empirically.
//
// FFT: standard iterative radix-2 DIF, std::complex<double> (no AVX2/SIMD).
// Convolution: cyclic, length N (N power of 2).
//   STD  : DFT(A)+DFT(B)+mul+IDFT  = 3 transforms
//   SPLIT: A=100*Ahi+Alo, B=100*Bhi+Blo; 4 DFT; freq-domain combine; 1 IDFT
//          = 5 transforms
//   C_hat[k] = 10000*(Ahi_hat[k]*Bhi_hat[k])
//            + 100*(Ahi_hat[k]*Blo_hat[k] + Alo_hat[k]*Bhi_hat[k])
//            + (Alo_hat[k]*Blo_hat[k])
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <complex>
#include <chrono>
#include <random>
#include <algorithm>

using cd = std::complex<double>;
static const double PI = acos(-1.0);

// ---- standard iterative radix-2 DIF FFT (in-place; bit-reversal at end) ----
static void fft(cd* a, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        double ang = -2.0 * PI / (double)len;
        cd wlen(cos(ang), sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd w(1, 0);
            for (size_t j = 0; j < half; j++) {
                cd u = a[i + j];
                cd v = a[i + j + half];
                a[i + j]        = u + v;
                a[i + j + half] = (u - v) * w;
                w *= wlen;
            }
        }
    }
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
}
static void ifft(cd* a, size_t n) {
    for (size_t i = 0; i < n; i++) a[i] = conj(a[i]);
    fft(a, n);
    cd inv(1.0 / (double)n, 0.0);
    for (size_t i = 0; i < n; i++) a[i] = conj(a[i]) * inv;
}

// ---- Method 1 (baseline): 2 DFT + mul + 1 IDFT  (3 transforms) ----
static void std_conv(const double* A, const double* B, size_t N, double* out) {
    static thread_local std::vector<cd> a, b;
    a.resize(N); b.resize(N);
    cd* pa = a.data(); cd* pb = b.data();
    for (size_t k = 0; k < N; k++) { pa[k] = cd(A[k], 0.0); pb[k] = cd(B[k], 0.0); }
    fft(pa, N);
    fft(pb, N);
    for (size_t k = 0; k < N; k++) pa[k] *= pb[k];
    ifft(pa, N);
    for (size_t k = 0; k < N; k++) out[k] = pa[k].real();
}

// ---- Method 2 (split-coeff): 4 DFT + freq combine + 1 IDFT  (5 transforms) ----
// A[n] = 100*Ahi[n] + Alo[n], B[n] = 100*Bhi[n] + Blo[n], all hi/lo in 0..99.
static void split_conv(const double* A, const double* B, size_t N, double* out) {
    static thread_local std::vector<cd> ahi, alo, bhi, blo, c;
    ahi.resize(N); alo.resize(N); bhi.resize(N); blo.resize(N); c.resize(N);
    cd* pah = ahi.data(); cd* pal = alo.data();
    cd* pbh = bhi.data(); cd* pbl = blo.data(); cd* pc = c.data();
    for (size_t k = 0; k < N; k++) {
        int ai = (int)A[k], bi = (int)B[k];
        int ah = ai / 100, al = ai % 100;
        int bh = bi / 100, bl = bi % 100;
        pah[k] = cd((double)ah, 0.0); pal[k] = cd((double)al, 0.0);
        pbh[k] = cd((double)bh, 0.0); pbl[k] = cd((double)bl, 0.0);
    }
    fft(pah, N); fft(pal, N); fft(pbh, N); fft(pbl, N);
    for (size_t k = 0; k < N; k++) {
        cd hh = pah[k] * pbh[k];
        cd hl = pah[k] * pbl[k];
        cd lh = pal[k] * pbh[k];
        cd ll = pal[k] * pbl[k];
        pc[k] = hh * 10000.0 + (hl + lh) * 100.0 + ll;
    }
    ifft(pc, N);
    for (size_t k = 0; k < N; k++) out[k] = pc[k].real();
}

// ---- brute-force cyclic convolution (reference) ----
static void brute_cyclic(const double* A, const double* B, size_t N, double* out) {
    std::fill(out, out + N, 0.0);
    for (size_t i = 0; i < N; i++) {
        double ai = A[i];
        if (ai == 0.0) continue;
        for (size_t j = 0; j < N; j++)
            out[(i + j) & (N - 1)] += ai * B[j];
    }
}

static double median_sorted(std::vector<double>& v) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

int main() {
    using clk = std::chrono::high_resolution_clock;
    auto T0 = clk::now();
    auto elapsed_ms = [&]() {
        return std::chrono::duration<double, std::milli>(clk::now() - T0).count();
    };

    printf("=== Split-coefficient FFT vs Standard FFT ===\n");
    printf("FFT: iterative radix-2 DIF, std::complex<double>. Conv: cyclic, length N (pow2).\n");
    printf("STD=2DFT+IDFT(3xform)  SPLIT=4DFT+combine+IDFT(5xform), limb=0..9999, hi/lo=0..99.\n");
    printf(" elems: random int 0..9999. median of 20 rounds. budget-guard @9s.\n\n");
    fflush(stdout);

    // ---- tiny sanity case (N=8) ----
    {
        size_t N = 8;
        double A8[8] = {1,2,3,4,5,6,7,8};
        double B8[8] = {9,8,7,6,5,4,3,2};
        double cb[8], cs[8], cp[8];
        brute_cyclic(A8, B8, N, cb);
        std_conv(A8, B8, N, cs);
        split_conv(A8, B8, N, cp);
        printf("[debug N=8] k: brute  std    split\n");
        for (size_t k = 0; k < N; k++)
            printf("           %2zu: %6.0f %6.0f %6.0f\n", k, cb[k], cs[k], cp[k]);
        double es = 0, ep = 0;
        for (size_t k = 0; k < N; k++) {
            es = std::max(es, std::abs(cs[k] - cb[k]));
            ep = std::max(ep, std::abs(cp[k] - cb[k]));
        }
        printf("[debug] max_err std=%.3e split=%.3e\n", es, ep);
        fflush(stdout);
    }

    std::mt19937 rng(20260731);

    struct S { size_t N; int rounds; };
    S sched[] = {
        {1u << 10, 20}, {1u << 12, 20}, {1u << 14, 20},
        {1u << 16, 20}, {1u << 18, 20},
    };

    printf("%8s %10s %10s %9s %10s %10s  %s\n",
           "N", "std_ms", "split_ms", "split/std", "std_err", "split_err", "correctness");
    fflush(stdout);

    for (auto& s : sched) {
        size_t N = s.N;
        int R = s.rounds;

        if (elapsed_ms() > 9000.0) {
            printf("%8zu %10s %10s %9s %10s %10s  SKIPPED(time-budget)\n",
                   N, "-", "-", "-", "-", "-");
            fflush(stdout);
            continue;
        }

        std::vector<double> A(N), B(N);
        for (size_t i = 0; i < N; i++) {
            A[i] = (double)(rng() % 10000);
            B[i] = (double)(rng() % 10000);
        }
        std::vector<double> cs(N), cp(N);

        // ---- correctness & error ----
        char corr[96];
        double std_err = 0, split_err = 0;
        bool have_brute = (N <= (1u << 14));
        if (have_brute) {
            std::vector<double> cb(N);
            brute_cyclic(A.data(), B.data(), N, cb.data());
            std_conv(A.data(), B.data(), N, cs.data());
            split_conv(A.data(), B.data(), N, cp.data());
            for (size_t k = 0; k < N; k++) {
                std_err   = std::max(std_err,  std::abs(cs[k] - cb[k]));
                split_err = std::max(split_err, std::abs(cp[k] - cb[k]));
            }
            bool ok = (std_err < 0.5) && (split_err < 0.5);
            snprintf(corr, sizeof(corr), "%s(brute,thr<0.5)", ok ? "PASS" : "FAIL");
        } else {
            // brute infeasible at this N: cross-check std vs split + fractional margin.
            std_conv(A.data(), B.data(), N, cs.data());
            split_conv(A.data(), B.data(), N, cp.data());
            double xchk = 0;
            for (size_t k = 0; k < N; k++) {
                std_err   = std::max(std_err,  std::abs(cs[k] - std::round(cs[k])));
                split_err = std::max(split_err, std::abs(cp[k] - std::round(cp[k])));
                xchk = std::max(xchk, std::abs(cs[k] - cp[k]));
            }
            snprintf(corr, sizeof(corr), "%s(xchk,%.1e)", xchk < 1.0 ? "PASS" : "FAIL", xchk);
        }

        // ---- benchmark: 1 warmup + R timed runs, median ----
        std_conv(A.data(), B.data(), N, cs.data());   // warmup
        std::vector<double> ts, tp;
        ts.reserve(R); tp.reserve(R);
        for (int r = 0; r < R; r++) {
            auto t1 = clk::now(); std_conv(A.data(), B.data(), N, cs.data()); auto t2 = clk::now();
            ts.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        }
        for (int r = 0; r < R; r++) {
            auto t1 = clk::now(); split_conv(A.data(), B.data(), N, cp.data()); auto t2 = clk::now();
            tp.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        }
        double mst = median_sorted(ts), mp = median_sorted(tp);
        double ratio = (mst > 0) ? mp / mst : 0;

        printf("%8zu %10.3f %10.3f %9.3f %10.2e %10.2e  %s\n",
               N, mst, mp, ratio, std_err, split_err, corr);
        fflush(stdout);
    }

    printf("\nLegend: split/std > 1.0 => split slower than standard (5 vs 3 transforms).\n");
    printf("err: vs brute force (N<=2^14); fractional margin |x-round(x)| (N>2^14).\n");
    printf("correctness: PASS if err<0.5 (brute) or xchk<1.0 (cross-check, large N).\n");
    return 0;
}
