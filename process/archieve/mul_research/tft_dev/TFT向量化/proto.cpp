// C++ TFT (Truncated Fourier Transform) prototype
// Validates TFT_DFT (input-pruned recursive DFT) + conjugate completion IDFT.
// Based on Python prototype validation.
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

// Standard iterative radix-2 DIF FFT (for reference and IDFT)
void fft(cd* a, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        double ang = -2.0 * PI / len;
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

void ifft(cd* a, size_t n) {
    for (size_t i = 0; i < n; i++) a[i] = conj(a[i]);
    fft(a, n);
    for (size_t i = 0; i < n; i++) a[i] = conj(a[i]) / cd(n, 0);
}

// Recursive TFT_DFT: compute first m coefficients of DFT_N(a)
// a: length-N complex array (zero-padded)
// N: power of 2
// m: 0 <= m <= N
// Output: first m elements of a are overwritten with DFT coefficients
// scratch: buffer of size >= 2*N, used with offset for recursion
// scratch_off: current offset into scratch (each level uses 2*h elements)
void tft_dft_rec(cd* a, size_t N, size_t m, cd* scratch, size_t scratch_off) {
    if (m == 0) return;
    if (N == 1) return;
    if (m == N) { fft(a, N); return; }

    size_t h = N / 2;
    // Use scratch at current offset for a_e and a_o
    cd* a_e = scratch + scratch_off;       // length h
    cd* a_o = scratch + scratch_off + h;   // length h
    for (size_t i = 0; i < h; i++) {
        a_e[i] = a[2 * i];
        a_o[i] = a[2 * i + 1];
    }

    size_t next_off = scratch_off + 2 * h;  // next level uses scratch after ours

    if (m <= h) {
        tft_dft_rec(a_e, h, m, scratch, next_off);
        tft_dft_rec(a_o, h, m, scratch, next_off);
        for (size_t k = 0; k < m; k++) {
            double ang = -2.0 * PI * k / N;
            cd w(cos(ang), sin(ang));
            a[k] = a_e[k] + w * a_o[k];
        }
    } else {
        tft_dft_rec(a_e, h, h, scratch, next_off);
        tft_dft_rec(a_o, h, h, scratch, next_off);
        for (size_t k = 0; k < h; k++) {
            double ang = -2.0 * PI * k / N;
            cd w(cos(ang), sin(ang));
            a[k] = a_e[k] + w * a_o[k];
        }
        for (size_t k = 0; k < m - h; k++) {
            double ang = -2.0 * PI * k / N;
            cd w(cos(ang), sin(ang));
            a[h + k] = a_e[k] - w * a_o[k];
        }
    }
}

void tft_dft(cd* a, size_t N, size_t m, std::vector<cd>& scratch) {
    // Total scratch: 2*(N/2 + N/4 + ...) = 2*N at most
    if (scratch.size() < 2 * N) scratch.resize(2 * N);
    tft_dft_rec(a, N, m, scratch.data(), 0);
}

// TFT-based multiplication (real-valued)
// a, b: real-valued arrays
// Returns: convolution of a and b (length la+lb-1)
void tft_mul(const double* a, size_t la, const double* b, size_t lb, double* out) {
    if (la == 0 || lb == 0) return;
    size_t L = la + lb - 1;
    size_t N = 1;
    while (N < L) N *= 2;

    // Pad to N
    std::vector<cd> ca(N, cd(0, 0)), cb(N, cd(0, 0));
    for (size_t i = 0; i < la; i++) ca[i] = cd(a[i], 0);
    for (size_t i = 0; i < lb; i++) cb[i] = cd(b[i], 0);

    // TFT_DFT: compute first L coefficients
    std::vector<cd> scratch(2 * N);
    tft_dft(ca.data(), N, L, scratch);
    tft_dft(cb.data(), N, L, scratch);

    // Pointwise multiply first L coefficients
    for (size_t k = 0; k < L; k++) ca[k] *= cb[k];

    // Conjugate completion: for real output, C[N-k] = conj(C[k])
    // Need C[m..N-1] where m = L. For k in [1, N-L]: C[N-k] = conj(C[k])
    // Requires L > N/2 (always true since N/2 < L <= N)
    for (size_t k = 1; k <= N - L; k++) {
        ca[N - k] = conj(ca[k]);
    }

    // Standard IFFT
    ifft(ca.data(), N);

    // Extract real part
    for (size_t i = 0; i < L; i++) out[i] = ca[i].real();
}

// Standard FFT multiplication for comparison
void fft_mul(const double* a, size_t la, const double* b, size_t lb, double* out) {
    if (la == 0 || lb == 0) return;
    size_t L = la + lb - 1;
    size_t N = 1;
    while (N < L) N *= 2;
    std::vector<cd> ca(N, cd(0, 0)), cb(N, cd(0, 0));
    for (size_t i = 0; i < la; i++) ca[i] = cd(a[i], 0);
    for (size_t i = 0; i < lb; i++) cb[i] = cd(b[i], 0);
    fft(ca.data(), N);
    fft(cb.data(), N);
    for (size_t i = 0; i < N; i++) ca[i] *= cb[i];
    ifft(ca.data(), N);
    for (size_t i = 0; i < L; i++) out[i] = ca[i].real();
}

// Brute-force convolution for verification
void brute_mul(const double* a, size_t la, const double* b, size_t lb, double* out) {
    size_t L = la + lb - 1;
    std::fill(out, out + L, 0.0);
    for (size_t i = 0; i < la; i++)
        for (size_t j = 0; j < lb; j++)
            out[i + j] += a[i] * b[j];
}

int main() {
    printf("=== C++ TFT Correctness Test ===\n");
    std::mt19937 rng(42);
    bool all_pass = true;

    // First verify fft_mul against brute force
    printf("--- fft_mul vs brute_mul ---\n");
    {
        size_t la = 5, lb = 5;
        std::vector<double> a(la), b(lb);
        for (size_t i = 0; i < la; i++) a[i] = rng() % 10000;
        for (size_t i = 0; i < lb; i++) b[i] = rng() % 10000;
        std::vector<double> c_fft(la+lb-1), c_brute(la+lb-1), c_tft(la+lb-1);
        fft_mul(a.data(), la, b.data(), lb, c_fft.data());
        brute_mul(a.data(), la, b.data(), lb, c_brute.data());
        tft_mul(a.data(), la, b.data(), lb, c_tft.data());
        printf("  L=%zu:\n", la+lb-1);
        for (size_t i = 0; i < la+lb-1; i++) {
            printf("    [%zu] brute=%.1f fft=%.1f tft=%.1f\n", i, c_brute[i], c_fft[i], c_tft[i]);
        }
        double err_fft = 0, err_tft = 0;
        for (size_t i = 0; i < la+lb-1; i++) {
            err_fft = std::max(err_fft, std::abs(c_brute[i] - c_fft[i]));
            err_tft = std::max(err_tft, std::abs(c_brute[i] - c_tft[i]));
        }
        printf("  fft err=%.2e, tft err=%.2e\n", err_fft, err_tft);
    }

    struct TC { size_t la, lb; };
    TC cases[] = {
        {1, 1}, {5, 5}, {10, 5}, {100, 100}, {255, 255},
        {256, 256}, {200, 300}, {1000, 1000}, {500, 1500},
        {127, 127}, {128, 128}, {129, 129}, {512, 512},
    };

    printf("\n--- Full test ---\n");
    for (auto& tc : cases) {
        std::vector<double> a(tc.la), b(tc.lb);
        for (size_t i = 0; i < tc.la; i++) a[i] = rng() % 10000;
        for (size_t i = 0; i < tc.lb; i++) b[i] = rng() % 10000;
        std::vector<double> c_fft(tc.la + tc.lb - 1), c_tft(tc.la + tc.lb - 1), c_brute(tc.la + tc.lb - 1);
        fft_mul(a.data(), tc.la, b.data(), tc.lb, c_fft.data());
        tft_mul(a.data(), tc.la, b.data(), tc.lb, c_tft.data());
        brute_mul(a.data(), tc.la, b.data(), tc.lb, c_brute.data());
        double err_fft = 0, err_tft = 0;
        for (size_t i = 0; i < tc.la + tc.lb - 1; i++) {
            err_fft = std::max(err_fft, std::abs(c_brute[i] - c_fft[i]));
            err_tft = std::max(err_tft, std::abs(c_brute[i] - c_tft[i]));
        }
        bool ok = err_tft < 1e-3;
        printf("  la=%4zu lb=%4zu L=%5zu: fft_err=%.2e tft_err=%.2e %s\n", tc.la, tc.lb, tc.la+tc.lb-1, err_fft, err_tft, ok ? "PASS" : "FAIL");
        if (!ok) all_pass = false;
    }

    if (!all_pass) { printf("FAILED\n"); return 1; }
    printf("All PASS\n\n");

    printf("=== TFT vs FFT Benchmark ===\n");
    printf("  %8s %8s %10s %10s %8s\n", "la", "lb", "fft(ms)", "tft(ms)", "ratio");
    fflush(stdout);

    TC bench[] = {
        {256, 256}, {512, 512}, {1024, 1024}, {2048, 2048},
        {4096, 4096}, {8192, 8192}, {16384, 16384}, {32768, 32768},
        {65536, 65536}, {131072, 131072}, {262144, 262144},
    };
    for (auto& tc : bench) {
        std::vector<double> a(tc.la), b(tc.lb);
        for (size_t i = 0; i < tc.la; i++) a[i] = rng() % 10000;
        for (size_t i = 0; i < tc.lb; i++) b[i] = rng() % 10000;
        std::vector<double> out(tc.la + tc.lb - 1);
        int R = (tc.la <= 4096) ? 50 : (tc.la <= 32768) ? 10 : 3;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < R; r++) fft_mul(a.data(), tc.la, b.data(), tc.lb, out.data());
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_fft = std::chrono::duration<double, std::milli>(t1 - t0).count() / R;

        t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < R; r++) tft_mul(a.data(), tc.la, b.data(), tc.lb, out.data());
        t1 = std::chrono::high_resolution_clock::now();
        double t_tft = std::chrono::duration<double, std::milli>(t1 - t0).count() / R;

        printf("  %8zu %8zu %10.3f %10.3f %8.3f\n", tc.la, tc.lb, t_fft, t_tft, t_tft / t_fft);
        fflush(stdout);
    }
    return 0;
}
