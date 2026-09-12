// NTT vs FFT performance comparison prototype
// Tests the claim: "NTT is often slower than FFT in software"
// Both use iterative radix-2 DIF with identical butterfly structure.
// NTT: p=998244353 (119*2^23+1), g=3
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <complex>
#include <algorithm>
#include <chrono>
#include <random>

using cd = std::complex<double>;
using u64 = uint64_t;
const double PI = acos(-1.0);

static constexpr u64 MOD = 998244353ULL;  // 119 * 2^23 + 1
static constexpr u64 G = 3;

u64 mod_pow(u64 a, u64 e) {
    u64 r = 1, b = a % MOD;
    while (e) {
        if (e & 1) r = r * b % MOD;
        b = b * b % MOD;
        e >>= 1;
    }
    return r;
}

// ===================== FFT (iterative radix-2 DIF) =====================
void fft(cd* a, size_t n) {
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

void ifft(cd* a, size_t n) {
    for (size_t i = 0; i < n; i++) a[i] = conj(a[i]);
    fft(a, n);
    double inv = 1.0 / (double)n;
    for (size_t i = 0; i < n; i++) a[i] = conj(a[i]) * inv;
}

// ===================== NTT (iterative radix-2 DIF, same structure) =====================
void ntt(u64* a, size_t n) {
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        u64 wlen = mod_pow(G, (MOD - 1) / len);
        for (size_t i = 0; i < n; i += len) {
            u64 w = 1;
            for (size_t j = 0; j < half; j++) {
                u64 u = a[i + j];
                u64 v = a[i + j + half];
                a[i + j] = (u + v) % MOD;
                a[i + j + half] = (u + MOD - v) % MOD * w % MOD;
                w = w * wlen % MOD;
            }
        }
    }
    // Bit reversal (same as FFT)
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
}

void intt(u64* a, size_t n) {
    // DIF with inverse roots (structure identical to forward NTT)
    for (size_t len = n; len > 1; len >>= 1) {
        size_t half = len >> 1;
        u64 wlen = mod_pow(G, (MOD - 1) - (MOD - 1) / len);
        for (size_t i = 0; i < n; i += len) {
            u64 w = 1;
            for (size_t j = 0; j < half; j++) {
                u64 u = a[i + j];
                u64 v = a[i + j + half];
                a[i + j] = (u + v) % MOD;
                a[i + j + half] = (u + MOD - v) % MOD * w % MOD;
                w = w * wlen % MOD;
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
    // Divide by n
    u64 n_inv = mod_pow(n, MOD - 2);
    for (size_t i = 0; i < n; i++) a[i] = a[i] * n_inv % MOD;
}

// ===================== Main =====================
int main() {
    using clock = std::chrono::high_resolution_clock;

    printf("=== NTT vs FFT Performance Comparison ===\n");
    printf("MOD=%llu, G=%llu\n\n", (unsigned long long)MOD, (unsigned long long)G);

    std::mt19937 rng(12345);

    // ---- Correctness verification ----
    printf("--- Correctness (cyclic conv, inputs 0-9999) ---\n");
    bool all_correct = true;
    {
        size_t test_sizes[] = {1u<<10, 1u<<12, 1u<<14};
        for (size_t N : test_sizes) {
            std::vector<u64> a(N), b(N);
            for (size_t i = 0; i < N; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }

            // FFT
            std::vector<cd> ca(N), cb(N);
            for (size_t i = 0; i < N; i++) {
                ca[i] = cd((double)a[i], 0.0);
                cb[i] = cd((double)b[i], 0.0);
            }
            fft(ca.data(), N); fft(cb.data(), N);
            for (size_t i = 0; i < N; i++) ca[i] *= cb[i];
            ifft(ca.data(), N);

            // NTT
            std::vector<u64> na(N), nb(N);
            for (size_t i = 0; i < N; i++) { na[i] = a[i]; nb[i] = b[i]; }
            ntt(na.data(), N); ntt(nb.data(), N);
            for (size_t i = 0; i < N; i++) na[i] = na[i] * nb[i] % MOD;
            intt(na.data(), N);

            // Compare: round(fft_real) % MOD == ntt
            int mismatches = 0;
            double max_err = 0;
            for (size_t i = 0; i < N; i++) {
                double fr = ca[i].real();
                long long ri = std::llround(fr);
                max_err = std::max(max_err, std::abs(fr - (double)ri));
                if ((u64)ri % MOD != na[i]) {
                    if (mismatches < 3)
                        printf("  N=%zu [%zu]: fft=%.1f r=%lld mod=%llu ntt=%llu\n",
                               N, i, fr, ri,
                               (unsigned long long)((u64)ri % MOD),
                               (unsigned long long)na[i]);
                    mismatches++;
                }
            }
            bool ok = (mismatches == 0);
            printf("  N=%5zu: %s (mismatch=%d, round_err=%.1e)\n",
                   N, ok ? "PASS" : "FAIL", mismatches, max_err);
            if (!ok) all_correct = false;
        }
    }

    // ---- Benchmark ----
    printf("\n--- Benchmark (median of 20, DFT+DOT+IDFT) ---\n");
    printf("  %8s %10s %10s %10s  %s\n", "size", "fft_ms", "ntt_ms", "ntt/fft", "status");
    fflush(stdout);

    const int ROUNDS = 20;
    size_t bench_sizes[] = {1u<<10, 1u<<12, 1u<<14, 1u<<16, 1u<<18, 1u<<20};

    for (size_t N : bench_sizes) {
        std::vector<u64> a(N), b(N);
        for (size_t i = 0; i < N; i++) { a[i] = rng() % 10000; b[i] = rng() % 10000; }

        // Correctness check for this size
        std::vector<cd> cak(N), cbk(N);
        std::vector<u64> nak(N), nbk(N);
        for (size_t i = 0; i < N; i++) {
            cak[i] = cd((double)a[i], 0.0); cbk[i] = cd((double)b[i], 0.0);
            nak[i] = a[i]; nbk[i] = b[i];
        }
        fft(cak.data(), N); fft(cbk.data(), N);
        for (size_t i = 0; i < N; i++) cak[i] *= cbk[i];
        ifft(cak.data(), N);
        ntt(nak.data(), N); ntt(nbk.data(), N);
        for (size_t i = 0; i < N; i++) nak[i] = nak[i] * nbk[i] % MOD;
        intt(nak.data(), N);
        bool correct = true;
        for (size_t i = 0; i < N; i++) {
            if ((u64)std::llround(cak[i].real()) % MOD != nak[i]) { correct = false; break; }
        }

        // FFT benchmark (2 warmup + 20 measured)
        std::vector<cd> ca(N), cb(N);
        std::vector<double> fft_times;
        for (int r = 0; r < ROUNDS + 2; r++) {
            for (size_t i = 0; i < N; i++) {
                ca[i] = cd((double)a[i], 0.0);
                cb[i] = cd((double)b[i], 0.0);
            }
            auto t0 = clock::now();
            fft(ca.data(), N);
            fft(cb.data(), N);
            for (size_t i = 0; i < N; i++) ca[i] *= cb[i];
            ifft(ca.data(), N);
            auto t1 = clock::now();
            if (r >= 2)
                fft_times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        // NTT benchmark (2 warmup + 20 measured)
        std::vector<u64> na(N), nb(N);
        std::vector<double> ntt_times;
        for (int r = 0; r < ROUNDS + 2; r++) {
            for (size_t i = 0; i < N; i++) { na[i] = a[i]; nb[i] = b[i]; }
            auto t0 = clock::now();
            ntt(na.data(), N);
            ntt(nb.data(), N);
            for (size_t i = 0; i < N; i++) na[i] = na[i] * nb[i] % MOD;
            intt(na.data(), N);
            auto t1 = clock::now();
            if (r >= 2)
                ntt_times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        std::sort(fft_times.begin(), fft_times.end());
        std::sort(ntt_times.begin(), ntt_times.end());
        double fft_med = (fft_times[ROUNDS/2 - 1] + fft_times[ROUNDS/2]) / 2.0;
        double ntt_med = (ntt_times[ROUNDS/2 - 1] + ntt_times[ROUNDS/2]) / 2.0;
        double ratio = ntt_med / fft_med;

        printf("  %8zu %10.3f %10.3f %10.3f  %s%s\n",
               N, fft_med, ntt_med, ratio,
               correct ? "OK" : "MISMATCH",
               ratio < 1.0 ? " *NTT FASTER*" : "");
        fflush(stdout);
    }

    printf("\nDone.\n");
    return 0;
}
