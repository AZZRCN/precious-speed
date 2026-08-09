// Interleaved DFT concept prototype
// Tests whether interleaving two DFTs at each butterfly level improves cache utilization.
// Uses simple iterative radix-2 FFT for clarity (not the split-radix from mul.cpp).
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <complex>
#include <chrono>
#include <random>

using cd = std::complex<double>;
const double PI = acos(-1.0);

// Simple iterative radix-2 DIF FFT
void dif_fft(cd* a, size_t n) {
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

// Interleaved DFT: process both arrays in the same butterfly loop
void dif_fft_interleaved(cd* a, cd* b, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        double ang = -2.0 * PI / len;
        cd wlen(cos(ang), sin(ang));
        for (size_t i = 0; i < n; i += len) {
            cd w(1, 0);
            for (size_t j = 0; j < half; j++) {
                // Process a
                cd ua = a[i + j];
                cd va = a[i + j + half];
                a[i + j] = ua + va;
                a[i + j + half] = (ua - va) * w;
                // Process b (same twiddle factor w)
                cd ub = b[i + j];
                cd vb = b[i + j + half];
                b[i + j] = ub + vb;
                b[i + j + half] = (ub - vb) * w;
                w *= wlen;
            }
        }
    }
    // Bit reversal for both
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(a[i], a[j]); std::swap(b[i], b[j]); }
    }
}

// Sequential 2xDFT
void two_dft_sequential(cd* a, cd* b, size_t n) {
    dif_fft(a, n);
    dif_fft(b, n);
}

// Verify correctness
bool verify(cd* a, cd* b, size_t n) {
    std::vector<cd> a_copy(a, a + n), b_copy(b, b + n);
    std::vector<cd> a_seq(a, a + n), b_seq(b, b + n);

    two_dft_sequential(a_seq.data(), b_seq.data(), n);

    // Reset a, b to original for interleaved
    std::copy(a_copy.begin(), a_copy.end(), a);
    std::copy(b_copy.begin(), b_copy.end(), b);
    dif_fft_interleaved(a, b, n);

    double err = 0;
    for (size_t i = 0; i < n; i++) {
        err = std::max(err, std::abs(a[i] - a_seq[i]));
        err = std::max(err, std::abs(b[i] - b_seq[i]));
    }
    return err < 1e-9;
}

int main() {
    printf("=== Interleaved DFT Correctness Test ===\n");
    std::mt19937 rng(42);
    bool all_pass = true;
    for (size_t n : {16, 64, 256, 1024, 4096}) {
        std::vector<cd> a(n), b(n);
        for (size_t i = 0; i < n; i++) {
            a[i] = cd(rng() % 10000, rng() % 10000);
            b[i] = cd(rng() % 10000, rng() % 10000);
        }
        bool ok = verify(a.data(), b.data(), n);
        printf("  n=%5zu: %s\n", n, ok ? "PASS" : "FAIL");
        if (!ok) all_pass = false;
    }
    if (!all_pass) { printf("FAILED\n"); return 1; }
    printf("All PASS\n\n");

    printf("=== Interleaved vs Sequential 2xDFT Benchmark ===\n");
    printf("  %8s %12s %12s %8s\n", "n", "seq(ms)", "inter(ms)", "ratio");
    fflush(stdout);

    for (size_t n : {1024, 4096, 16384, 65536, 262144, 524288, 1048576}) {
        std::vector<cd> a(n), b(n);
        for (size_t i = 0; i < n; i++) {
            a[i] = cd(rng() % 10000, rng() % 10000);
            b[i] = cd(rng() % 10000, rng() % 10000);
        }

        int R = (n <= 65536) ? 50 : (n <= 262144) ? 10 : 3;
        // Sequential
        std::vector<cd> a1(a), b1(b);
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < R; r++) {
            std::copy(a.begin(), a.end(), a1.begin());
            std::copy(b.begin(), b.end(), b1.begin());
            two_dft_sequential(a1.data(), b1.data(), n);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_seq = std::chrono::duration<double, std::milli>(t1 - t0).count() / R;

        // Interleaved
        std::vector<cd> a2(a), b2(b);
        t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < R; r++) {
            std::copy(a.begin(), a.end(), a2.begin());
            std::copy(b.begin(), b.end(), b2.begin());
            dif_fft_interleaved(a2.data(), b2.data(), n);
        }
        t1 = std::chrono::high_resolution_clock::now();
        double t_inter = std::chrono::duration<double, std::milli>(t1 - t0).count() / R;

        double ratio = t_inter / t_seq;
        printf("  %8zu %12.3f %12.3f %8.3f\n", n, t_seq, t_inter, ratio);
        fflush(stdout);
    }
    return 0;
}
