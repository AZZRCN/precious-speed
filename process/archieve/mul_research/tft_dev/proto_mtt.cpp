// proto_mtt.cpp — MTT conjugate vs Standard vs RIRI convolution benchmark.
//
// Question: does the MTT "two-for-one" DFT (pack P=A+iB, 1 DFT, extract DFT(A)
// and DFT(B) via conjugate symmetry) actually beat the standard 2-DFT approach
// for real convolution? The project's mul.cpp uses RIRI packing (same P=A+iB
// packing, but a FUSED pointwise product with no explicit extraction).
//
// FFT: standard iterative radix-2 DIF, std::complex<double> (no AVX2).
// Convolution: cyclic, length N (N power of 2). All 3 methods compute the
// same result; only transform count + memory traffic differ.
//   STD  : DFT(A)+DFT(B)+mul+IDFT  = 3 transforms
//   MTT  : DFT(P=A+iB), extract Ahat/Bhat into 2 arrays, mul, IDFT = 2 transforms
//   RIRI : DFT(P=A+iB), FUSED mul (no extraction arrays), IDFT = 2 transforms
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

// ---- Method 2 (baseline): 2 DFT + mul + 1 IDFT  (3 transforms) ----
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

// ---- Method 1 (MTT): P=A+iB, 1 DFT, extract via conjugate symmetry, mul, 1 IDFT (2 transforms) ----
//   Ahat[k] = (p[k] + conj(p[(N-k)%N])) / 2
//   Bhat[k] = (p[k] - conj(p[(N-k)%N])) / (2i)      (1/(2i) = -i/2)
// Explicitly materializes Ahat and Bhat (extra memory traffic).
static void mtt_conv(const double* A, const double* B, size_t N, double* out) {
    static thread_local std::vector<cd> p, Ahat, Bhat, chat;
    p.resize(N); Ahat.resize(N); Bhat.resize(N); chat.resize(N);
    cd* pp = p.data(); cd* pa = Ahat.data(); cd* pb = Bhat.data(); cd* pc = chat.data();
    for (size_t k = 0; k < N; k++) pp[k] = cd(A[k], B[k]);
    fft(pp, N);
    const cd mulB(0.0, -0.5);  // 1/(2i) = -i/2
    for (size_t k = 0; k < N; k++) {
        size_t idx = (N - k) & (N - 1);     // (N-k) mod N, N is power of 2
        cd pj = conj(pp[idx]);
        pa[k] = (pp[k] + pj) * 0.5;
        pb[k] = (pp[k] - pj) * mulB;
    }
    for (size_t k = 0; k < N; k++) pc[k] = pa[k] * pb[k];
    ifft(pc, N);
    for (size_t k = 0; k < N; k++) out[k] = pc[k].real();
}

// ---- Method 3 (RIRI): P=A+iB, 1 DFT, FUSED pointwise mul, 1 IDFT (2 transforms) ----
// Algebra: chat[k] = Ahat[k]*Bhat[k]
//        = (p+pc)/2 * (p-pc)*(-i/2) = (-i/4) * (p^2 - pc^2)
//   => chat[k] = -i/4 * (p[k]^2 - conj(p[(N-k)%N])^2)
// In-place: process conjugate-symmetric pairs (k, N-k) together so neither slot
// is overwritten before both are consumed (mirrors mul.cpp's fused real_dot).
static void riri_conv(const double* A, const double* B, size_t N, double* out) {
    static thread_local std::vector<cd> p;
    p.resize(N);
    cd* pp = p.data();
    for (size_t k = 0; k < N; k++) pp[k] = cd(A[k], B[k]);
    fft(pp, N);
    const cd coef(0.0, -0.25);  // -i/4
    for (size_t k = 0; k <= N / 2; k++) {
        size_t idx = (N - k) & (N - 1);
        if (idx == k) {
            cd pk = pp[k];
            cd pcj = conj(pk);
            pp[k] = coef * (pk * pk - pcj * pcj);
        } else {
            cd pk = pp[k];
            cd q  = pp[idx];
            cd pk2 = pk * pk;
            cd q2  = q * q;
            cd qc  = conj(q);  qc  = qc * qc;     // conj(q)^2
            cd pc  = conj(pk); pc  = pc * pc;      // conj(pk)^2
            pp[k]   = coef * (pk2 - qc);
            pp[idx] = coef * (q2  - pc);
        }
    }
    ifft(pp, N);
    for (size_t k = 0; k < N; k++) out[k] = pp[k].real();
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

static double max_abs(const double* x, size_t N) {
    double m = 0;
    for (size_t i = 0; i < N; i++) m = std::max(m, std::abs(x[i]));
    return m;
}
static double rel_err(const double* a, const double* b, size_t N) {
    double denom = std::max(1.0, max_abs(b, N));
    double e = 0;
    for (size_t i = 0; i < N; i++) e = std::max(e, std::abs(a[i] - b[i]));
    return e / denom;
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

    printf("=== MTT conjugate vs Standard vs RIRI ===\n");
    printf("FFT: iterative radix-2 DIF, std::complex<double>. Conv: cyclic, length N (pow2).\n");
    printf("STD=2DFT+IDFT(3xform)  MTT=P(A+iB),1DFT,extract,mul,IDFT(2xform)  RIRI=fused(2xform)\n");
    printf(" elems: random int 0..9999. median of R rounds. budget-guard @8s.\n\n");
    fflush(stdout);

    // ---- tiny sanity case (N=8) ----
    {
        size_t N = 8;
        double A8[8] = {1,2,3,4,5,6,7,8};
        double B8[8] = {9,8,7,6,5,4,3,2};
        double cb[8], cs[8], cm[8], cr[8];
        brute_cyclic(A8, B8, N, cb);
        std_conv(A8, B8, N, cs);
        mtt_conv(A8, B8, N, cm);
        riri_conv(A8, B8, N, cr);
        printf("[debug N=8] k: brute  std    mtt    riri\n");
        for (size_t k = 0; k < N; k++)
            printf("           %2zu: %6.0f %6.0f %6.0f %6.0f\n", k, cb[k], cs[k], cm[k], cr[k]);
        printf("[debug] rel_err std=%.3e mtt=%.3e riri=%.3e\n",
               rel_err(cs,cb,N), rel_err(cm,cb,N), rel_err(cr,cb,N));
        fflush(stdout);
    }


    std::mt19937 rng(20260731);

    struct S { size_t N; int rounds; };
    // Rounds reduced at large N to fit the ~10s wall budget (see note in report).
    S sched[] = {
        {1u << 10, 20}, {1u << 12, 20}, {1u << 14, 20},
        {1u << 16, 20}, {1u << 18, 10}, {1u << 20, 5},
    };

    printf("%8s %6s %10s %10s %10s %9s %9s  %s\n",
           "N", "rounds", "mtt_ms", "std_ms", "riri_ms", "mtt/std", "riri/std", "correctness(ref,err)");
    fflush(stdout);

    for (auto& s : sched) {
        size_t N = s.N;
        int R = s.rounds;

        if (elapsed_ms() > 8000.0) {
            printf("%8zu %6d %10s %10s %10s %9s %9s  SKIPPED(time-budget)\n",
                   N, R, "-", "-", "-", "-", "-");
            fflush(stdout);
            continue;
        }

        std::vector<double> A(N), B(N);
        for (size_t i = 0; i < N; i++) {
            A[i] = (double)(rng() % 10000);
            B[i] = (double)(rng() % 10000);
        }
        std::vector<double> cm(N), cs(N), cr(N);

        // ---- correctness ----
        char corr[80];
        if (N <= (1u << 14)) {
            std::vector<double> cb(N);
            brute_cyclic(A.data(), B.data(), N, cb.data());
            mtt_conv(A.data(), B.data(), N, cm.data());
            std_conv(A.data(), B.data(), N, cs.data());
            riri_conv(A.data(), B.data(), N, cr.data());
            double worst = std::max(rel_err(cm.data(), cb.data(), N),
                          std::max(rel_err(cs.data(), cb.data(), N),
                                   rel_err(cr.data(), cb.data(), N)));
            snprintf(corr, sizeof(corr), "%s(brute,%.1e)", worst < 1e-3 ? "PASS" : "FAIL", worst);
        } else {
            // cross-check MTT & RIRI vs standard (standard verified at small N)
            std_conv(A.data(), B.data(), N, cs.data());
            mtt_conv(A.data(), B.data(), N, cm.data());
            riri_conv(A.data(), B.data(), N, cr.data());
            double worst = std::max(rel_err(cm.data(), cs.data(), N),
                                    rel_err(cr.data(), cs.data(), N));
            snprintf(corr, sizeof(corr), "%s(xstd,%.1e)", worst < 1e-3 ? "PASS" : "FAIL", worst);
        }

        // ---- benchmark: 1 warmup + R timed runs, median ----
        std_conv(A.data(), B.data(), N, cs.data());   // warmup
        std::vector<double> tm, ts, tr;
        tm.reserve(R); ts.reserve(R); tr.reserve(R);
        for (int r = 0; r < R; r++) {
            auto t1 = clk::now(); mtt_conv(A.data(), B.data(), N, cm.data()); auto t2 = clk::now();
            tm.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        }
        for (int r = 0; r < R; r++) {
            auto t1 = clk::now(); std_conv(A.data(), B.data(), N, cs.data()); auto t2 = clk::now();
            ts.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        }
        for (int r = 0; r < R; r++) {
            auto t1 = clk::now(); riri_conv(A.data(), B.data(), N, cr.data()); auto t2 = clk::now();
            tr.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
        }
        double mm = median_sorted(tm), mst = median_sorted(ts), mr = median_sorted(tr);
        double r_mtt  = (mst > 0) ? mm / mst : 0;
        double r_riri = (mst > 0) ? mr / mst : 0;

        printf("%8zu %6d %10.3f %10.3f %10.3f %9.3f %9.3f  %s\n",
               N, R, mm, mst, mr, r_mtt, r_riri, corr);
        fflush(stdout);
    }

    printf("\nLegend: ratio<1.0 => faster than standard. MTT/RIRI=2 transforms vs STD=3.\n");
    printf("MTT extracts Ahat/Bhat into 2 extra arrays (more mem traffic); RIRI fuses (1 in-place pass).\n");
    return 0;
}
