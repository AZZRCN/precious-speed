// double-double FFT prototype: measure overhead ratio vs double FFT
// Compile: g++ -std=c++20 -O2 -march=native -o bench_dd_fft.exe bench_dd_fft.cpp
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <complex>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==================== Double-Double arithmetic ====================
struct DD { double hi, lo; };

static inline DD two_sum(double a, double b) {
    double s = a + b;
    double v = s - a;
    double e = (a - (s - v)) + (b - v);
    return {s, e};
}
static inline DD fast_two_sum(double a, double b) {
    double s = a + b;
    double e = b - (s - a);
    return {s, e};
}
static inline DD two_prod(double a, double b) {
    double p = a * b;
    double e = __builtin_fma(a, b, -p);
    return {p, e};
}
static inline DD dd_add(DD a, DD b) {
    DD s = two_sum(a.hi, b.hi);
    DD t = two_sum(a.lo, b.lo);
    s.lo += t.hi;
    s = fast_two_sum(s.hi, s.lo);
    s.lo += t.lo;
    s = fast_two_sum(s.hi, s.lo);
    return s;
}
static inline DD dd_sub(DD a, DD b) { return dd_add(a, {-b.hi, -b.lo}); }
static inline DD dd_mul(DD a, DD b) {
    DD p = two_prod(a.hi, b.hi);
    p.lo += a.hi * b.lo + a.lo * b.hi;
    p = fast_two_sum(p.hi, p.lo);
    return p;
}

// ==================== Complex types ====================
using CD = std::complex<double>;
struct CDD { DD re, im; };
static inline CDD cdd_add(CDD a, CDD b) { return {dd_add(a.re, b.re), dd_add(a.im, b.im)}; }
static inline CDD cdd_sub(CDD a, CDD b) { return {dd_sub(a.re, b.re), dd_sub(a.im, b.im)}; }
static inline CDD cdd_mul(CDD a, CDD b) {
    DD ac = dd_mul(a.re, b.re);
    DD bd = dd_mul(a.im, b.im);
    DD ad = dd_mul(a.re, b.im);
    DD bc = dd_mul(a.im, b.re);
    return {dd_sub(ac, bd), dd_add(ad, bc)};
}
static inline CDD cdd_conj(CDD a) { return {a.re, {-a.im.hi, -a.im.lo}}; }

// ==================== Iterative FFT (Cooley-Tukey) ====================
// Simple non-SIMD iterative FFT for fair overhead comparison

static void fft_double(CD* a, size_t n) {
    // bit-reversal
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        CD wlen(std::cos(ang), std::sin(ang));
        size_t half = len >> 1;
        for (size_t i = 0; i < n; i += len) {
            CD w(1, 0);
            for (size_t j = 0; j < half; j++) {
                CD u = a[i + j];
                CD v = a[i + j + half] * w;
                a[i + j] = u + v;
                a[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
}

static void fft_dd(CDD* a, size_t n) {
    // bit-reversal
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        CDD wlen = {{std::cos(ang), 0}, {std::sin(ang), 0}};
        size_t half = len >> 1;
        for (size_t i = 0; i < n; i += len) {
            CDD w = {{1, 0}, {0, 0}};
            for (size_t j = 0; j < half; j++) {
                CDD u = a[i + j];
                CDD v = cdd_mul(w, a[i + j + half]);
                a[i + j] = cdd_add(u, v);
                a[i + j + half] = cdd_sub(u, v);
                w = cdd_mul(w, wlen);
            }
        }
    }
}

// ==================== Benchmark ====================
int main(int argc, char** argv) {
    size_t pow = (argc > 1) ? atoi(argv[1]) : 18;  // default 2^18
    size_t iters = (argc > 2) ? atoi(argv[2]) : 5;
    size_t n = size_t(1) << pow;

    printf("=== DD FFT overhead test ===\n");
    printf("n = 2^%zu = %zu, iters = %zu\n", pow, n, iters);

    std::vector<CD> a(n), b(n);
    std::vector<CDD> c(n), d(n);

    // Generate random input (simulate limbs 0-9999)
    srand(42);
    for (size_t i = 0; i < n; i++) {
        double v = double(rand() % 10000);
        a[i] = CD(v, 0);
        b[i] = CD(v, 0);
        c[i] = {{v, 0}, {0, 0}};
        d[i] = {{v, 0}, {0, 0}};
    }

    // Warmup
    fft_double(a.data(), n);
    fft_dd(c.data(), n);

    // Regenerate (fft is in-place)
    for (size_t i = 0; i < n; i++) {
        double v = double(rand() % 10000);
        a[i] = CD(v, 0);
        b[i] = CD(v, 0);
        c[i] = {{v, 0}, {0, 0}};
        d[i] = {{v, 0}, {0, 0}};
    }

    // Benchmark double FFT (forward + pointwise mul + inverse)
    double best_d = 1e9;
    for (size_t it = 0; it < iters; it++) {
        // Regenerate
        for (size_t i = 0; i < n; i++) {
            double v = double(rand() % 10000);
            a[i] = CD(v, 0);
            b[i] = CD(v, 0);
        }
        auto t0 = std::chrono::high_resolution_clock::now();
        fft_double(a.data(), n);
        fft_double(b.data(), n);
        for (size_t i = 0; i < n; i++) a[i] *= b[i];
        // inverse FFT
        for (size_t i = 0; i < n; i++) a[i] = std::conj(a[i]);
        fft_double(a.data(), n);
        for (size_t i = 0; i < n; i++) a[i] /= double(n);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        best_d = std::min(best_d, ms);
    }

    // Benchmark DD FFT (forward + pointwise mul + inverse)
    double best_dd = 1e9;
    for (size_t it = 0; it < iters; it++) {
        for (size_t i = 0; i < n; i++) {
            double v = double(rand() % 10000);
            c[i] = {{v, 0}, {0, 0}};
            d[i] = {{v, 0}, {0, 0}};
        }
        auto t0 = std::chrono::high_resolution_clock::now();
        fft_dd(c.data(), n);
        fft_dd(d.data(), n);
        for (size_t i = 0; i < n; i++) c[i] = cdd_mul(c[i], d[i]);
        // inverse FFT (conj + fft + scale)
        for (size_t i = 0; i < n; i++) c[i] = cdd_conj(c[i]);
        fft_dd(c.data(), n);
        DD inv_n = {1.0 / n, 0};
        for (size_t i = 0; i < n; i++) {
            c[i].re = dd_mul(c[i].re, inv_n);
            c[i].im = dd_mul(c[i].im, inv_n);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        best_dd = std::min(best_dd, ms);
    }

    printf("\nResults (best of %zu iters, full conv: 2x forward + pointwise + inverse):\n", iters);
    printf("  double FFT: %7.1f ms\n", best_d);
    printf("  DD FFT:     %7.1f ms\n", best_dd);
    printf("  ratio:      %.2fx slower\n", best_dd / best_d);

    // Verify correctness (compare first few outputs)
    printf("\nCorrectness check (first 3 conv outputs):\n");
    for (size_t i = 0; i < 3 && i < n; i++) {
        printf("  [%zu] double=%.1f  DD=%.1f+%.1f\n", i, a[i].real(), c[i].re.hi, c[i].re.lo);
    }

    return 0;
}
