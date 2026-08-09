// Six-step FFT cache optimization prototype
// Compares standard iterative radix-2 DIF FFT with GMP-style six-step FFT
// for large FFT sizes where cache locality becomes the bottleneck.
//
// Six-step FFT decomposes N = N1 * N2 (N1 ~= N2 ~= sqrt(N)) into:
//   1. Transpose input  (N1 x N2 -> N2 x N1)
//   2. N2 FFTs of length N1 (on contiguous rows)
//   3. Twiddle factor multiplication W_N^(n2*k1)
//   4. Transpose        (N2 x N1 -> N1 x N2)
//   5. N1 FFTs of length N2 (on contiguous rows)
//   6. Transpose        (N1 x N2 -> N2 x N1, final output order)
//
// DFT decomposition: n = N2*n1 + n2, k = N1*k2 + k1
//   X[k1,k2] = sum_{n2} W_N2^(n2*k2) * W_N^(n2*k1) * [sum_{n1} x[n1,n2] * W_N1^(n1*k1)]

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <complex>
#include <chrono>
#include <random>
#include <algorithm>

using cd = std::complex<double>;
const double PI = acos(-1.0);

// ============================================================
// Standard iterative radix-2 DIF FFT (baseline)
// In-place, output in normal (bit-reversed-corrected) order.
// ============================================================
void dif_fft(cd* a, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        double ang = -2.0 * PI / (double)len;
        cd wlen(cos(ang), sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd w(1, 0);
            for (size_t j = 0; j < half; j++) {
                cd u = a[i + j];
                cd v = a[i + j + half];
                a[i + j] = u + v;
                a[i + j + half] = (u - v) * w;
                w *= wlen;
            }
        }
    }
    // Bit reversal
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
}

// ============================================================
// Blocked matrix transpose: R x C (row-major) -> C x R (row-major)
// Blocking (B x B) keeps working set in L1/L2 cache.
// ============================================================
void transpose_blocked(const cd* src, cd* dst, size_t R, size_t C) {
    const size_t B = 32;
    for (size_t i = 0; i < R; i += B) {
        size_t ie = std::min(i + B, R);
        for (size_t j = 0; j < C; j += B) {
            size_t je = std::min(j + B, C);
            for (size_t ii = i; ii < ie; ii++) {
                for (size_t jj = j; jj < je; jj++) {
                    dst[jj * R + ii] = src[ii * C + jj];
                }
            }
        }
    }
}

// ============================================================
// Six-step FFT
// buf is a reusable scratch buffer (sized >= N).
// ============================================================
void six_step_fft(cd* a, size_t N, std::vector<cd>& buf) {
    if (N <= 1) return;

    // Decompose N = N1 * N2, N1 ~= N2, both powers of 2, N1 <= N2
    size_t e = 0, tmp = N;
    while (tmp > 1) { tmp >>= 1; e++; }
    size_t N1 = size_t(1) << (e / 2);
    size_t N2 = N / N1;

    if (buf.size() < N) buf.resize(N);
    cd* t = buf.data();

    // Step 1: Transpose a (N1 x N2) -> t (N2 x N1)
    //   t[n2*N1 + n1] = x[n1, n2]
    transpose_blocked(a, t, N1, N2);

    // Step 2: N2 FFTs of length N1 on rows of t
    //   t[n2*N1 + k1] = Y[n2, k1] = DFT_N1 over n1 of x[n1, n2]
    for (size_t i = 0; i < N2; i++)
        dif_fft(t + i * N1, N1);

    // Step 3: Twiddle factors W_N^(n2*k1)
    //   n2 = row index i, k1 = column index j
    //   Use recurrence w *= W_N^(n2), but recompute from cos/sin every
    //   CHUNK steps to limit floating-point error accumulation.
    //   (Without this, N=2^20 error exceeds 1e-6 due to error amplification
    //    in the second FFT stage.)
    const size_t CHUNK = 32;
    for (size_t i = 0; i < N2; i++) {
        double ang = -2.0 * PI * (double)i / (double)N;
        cd wbase(std::cos(ang), std::sin(ang));
        cd* row = t + i * N1;
        for (size_t j0 = 0; j0 < N1; j0 += CHUNK) {
            size_t j1 = std::min(j0 + CHUNK, N1);
            double ang0 = -2.0 * PI * (double)(i * j0) / (double)N;
            cd w(std::cos(ang0), std::sin(ang0));
            for (size_t j = j0; j < j1; j++) {
                row[j] *= w;
                w *= wbase;
            }
        }
    }
    //   t[n2*N1 + k1] = Z[n2, k1] = Y[n2, k1] * W_N^(n2*k1)

    // Step 4: Transpose t (N2 x N1) -> a (N1 x N2)
    //   a[k1*N2 + n2] = Z[n2, k1]
    transpose_blocked(t, a, N2, N1);

    // Step 5: N1 FFTs of length N2 on rows of a
    //   a[k1*N2 + k2] = X[k1, k2] = DFT_N2 over n2 of Z[n2, k1]
    for (size_t i = 0; i < N1; i++)
        dif_fft(a + i * N2, N2);

    // Step 6: Transpose a (N1 x N2) -> t (N2 x N1) for correct output order
    //   t[k2*N1 + k1] = a[k1*N2 + k2] = X[k1, k2]
    //   Index in t: k2*N1 + k1 = N1*k2 + k1 = k  (correct linear order)
    transpose_blocked(a, t, N1, N2);
    std::copy(t, t + N, a);
}

// ============================================================
// Correctness verification: compare six-step FFT with standard FFT
// ============================================================
bool verify_correctness(size_t N) {
    std::mt19937 rng(12345);
    std::vector<cd> a_std(N), a_six(N);
    // Input range [0,1): keeps absolute error well below 1e-6 at N=2^20
    // (FFT roundoff difference between two algorithms scales with input magnitude)
    for (size_t i = 0; i < N; i++) {
        double re = (double)(rng() % 10000) / 10000.0;
        double im = (double)(rng() % 10000) / 10000.0;
        a_std[i] = cd(re, im);
        a_six[i] = cd(re, im);
    }

    dif_fft(a_std.data(), N);
    std::vector<cd> buf;
    six_step_fft(a_six.data(), N, buf);

    double max_err = 0;
    for (size_t i = 0; i < N; i++)
        max_err = std::max(max_err, std::abs(a_std[i] - a_six[i]));

    bool ok = max_err < 1e-6;
    printf("  N=%7zu: max_err=%.2e  %s\n", N, max_err, ok ? "PASS" : "FAIL");
    return ok;
}

// ============================================================
// Benchmark: median of R runs, DFT only (copy outside timing)
// ============================================================
double benchmark_median(cd* input, size_t N, int R, bool use_six_step) {
    std::vector<cd> work(N);
    std::vector<cd> buf;
    std::vector<double> times(R);

    for (int r = 0; r < R; r++) {
        std::copy(input, input + N, work.begin());
        auto t0 = std::chrono::high_resolution_clock::now();
        if (use_six_step)
            six_step_fft(work.data(), N, buf);
        else
            dif_fft(work.data(), N);
        auto t1 = std::chrono::high_resolution_clock::now();
        times[r] = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    std::sort(times.begin(), times.end());
    return times[R / 2];
}

int main() {
    printf("=== Six-step FFT Cache Optimization Prototype ===\n\n");

    // --- Correctness ---
    printf("--- Correctness Test ---\n");
    bool all_pass = true;
    for (size_t N : {4, 16, 64, 256, 1024, 4096, 65536, 262144, 1048576}) {
        if (!verify_correctness(N)) all_pass = false;
    }
    if (!all_pass) { printf("\nFAILED\n"); return 1; }
    printf("All correctness tests PASSED\n\n");

    // --- Benchmark ---
    printf("--- Benchmark (median of 10 runs, forward DFT only) ---\n");
    printf("  %10s  %12s  %12s  %10s  %s\n",
           "size", "standard_ms", "sixstep_ms", "speedup", "correctness");
    fflush(stdout);

    std::mt19937 rng(42);
    for (size_t N : {(size_t)1 << 16, (size_t)1 << 18, (size_t)1 << 20}) {
        std::vector<cd> input(N);
        for (size_t i = 0; i < N; i++) {
            double re = (double)(rng() % 10000) / 10000.0;
            double im = (double)(rng() % 10000) / 10000.0;
            input[i] = cd(re, im);
        }

        int R = 10;
        double t_std = benchmark_median(input.data(), N, R, false);
        double t_six = benchmark_median(input.data(), N, R, true);

        // Verify correctness on this exact data
        std::vector<cd> a1(input), a2(input), buf;
        dif_fft(a1.data(), N);
        six_step_fft(a2.data(), N, buf);
        double err = 0;
        for (size_t i = 0; i < N; i++)
            err = std::max(err, std::abs(a1[i] - a2[i]));

        double speedup = t_std / t_six;
        printf("  %10zu  %12.3f  %12.3f  %9.3fx  %s\n",
               N, t_std, t_six, speedup, err < 1e-6 ? "OK" : "FAIL");
        fflush(stdout);
    }

    return 0;
}
