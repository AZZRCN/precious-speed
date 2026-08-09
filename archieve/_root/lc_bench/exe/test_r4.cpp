// Scalar equivalence test: fused radix-4 vs the EXISTING radix-2 DIF/DIT of mul.cpp.
//
// mul.cpp does NOT use a textbook radix-4-able FFT: its butterfly carries a
// per-BLOCK constant twiddle taken from a bit-reversed table, and the R2C
// pointwise multiply depends on that exact bit-reversed output order.
// So instead of writing "a radix-4 FFT", we algebraically FUSE two consecutive
// stages of *this* butterfly. Output order and twiddle table stay identical,
// hence frequencyDomainPointwiseMultiply keeps working untouched.
//
// tw[b] = exp(+2*pi*i * rev_{m-1}(b) / n)  =>  with w = tw[2b]:
//     tw[b]      = w^2      (stage A twiddle)
//     tw[2b]     = w        (stage B, low sub-block)
//     tw[2b+1]   = i*w      (stage B, high sub-block)
// Fused DIF (3 complex mults / 4 points instead of 4):
//     X=A, Y=B*w, Z=C*w^2, T=D*w^3
//     out[k]=(X+Z)+(Y+T)   out[k+q]=(X+Z)-(Y+T)
//     out[k+2q]=(X-Z)+i(Y-T)   out[k+3q]=(X-Z)-i(Y-T)
// Fused DIT (mirror, conjugate twiddles):
//     s0=A+B, s1=A-B, s2=C+D, s3=C-D, i3=i*s3
//     out[k]=s0+s2   out[k+q]=(s1-i3)*conj(w)
//     out[k+2q]=(s0-s2)*conj(w^2)   out[k+3q]=(s1+i3)*conj(w^3)
#include <complex>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <algorithm>

using cd = std::complex<double>;
using std::vector;

static vector<cd> tw;

// Exact scalar replica of TransformHelper::resize() (single-shot build).
static void buildTwiddle(std::uint32_t n) {
    std::uint32_t m = (std::uint32_t)std::__lg(n);
    std::uint32_t halfLog = m >> 1, halfSize = 1u << halfLog;
    vector<cd> baseFactors((size_t)halfSize << 1);
    const double angleStep = std::acos(-1.0) / halfSize, fineAngleStep = angleStep / halfSize;
    for (std::uint32_t i = 0, j = (halfSize * 3) >> 1, phaseAccumulator = 0; i != halfSize;
         phaseAccumulator -= halfSize - (j >> __builtin_ctz(++i))) {
        baseFactors[i] = std::polar(1.0, phaseAccumulator * angleStep);
        baseFactors[i | halfSize] = std::polar(1.0, phaseAccumulator * fineAngleStep);
    }
    tw.assign(n >> 1, cd(0, 0));
    tw[0] = cd(1, 0);
    for (std::uint32_t i = 1; i != (n >> 1); ++i)
        tw[i] = baseFactors[i & (halfSize - 1)] * baseFactors[halfSize | (i >> halfLog)];
}

// ---- scalar replica of the existing radix-2 kernels -------------------------
static void dif2(vector<cd>& a, std::uint32_t n) {
    for (std::uint32_t blockSize = n >> 1, stepSize = n; blockSize; stepSize = blockSize, blockSize >>= 1) {
        for (std::uint32_t p = 0; p != blockSize; ++p) {
            cd e = a[p], o = a[p + blockSize];
            a[p] = e + o; a[p + blockSize] = e - o;
        }
        std::uint32_t wi = 1;
        for (std::uint32_t base = stepSize; base != n; base += stepSize, ++wi) {
            cd w = tw[wi];
            for (std::uint32_t p = base; p != base + blockSize; ++p) {
                cd e = a[p], o = a[p + blockSize] * w;
                a[p] = e + o; a[p + blockSize] = e - o;
            }
        }
    }
}
static void dit2(vector<cd>& a, std::uint32_t n) {
    for (std::uint32_t blockSize = 1, stepSize = 2; blockSize != n; blockSize = stepSize, stepSize <<= 1) {
        for (std::uint32_t p = 0; p != blockSize; ++p) {
            cd e = a[p], o = a[p + blockSize];
            a[p] = e + o; a[p + blockSize] = e - o;
        }
        std::uint32_t wi = 1;
        for (std::uint32_t base = stepSize; base != n; base += stepSize, ++wi) {
            cd w = tw[wi];
            for (std::uint32_t p = base; p != base + blockSize; ++p) {
                cd e = a[p], o = a[p + blockSize];
                a[p] = e + o; a[p + blockSize] = (e - o) * std::conj(w);
            }
        }
    }
}

// ---- fused radix-4 ----------------------------------------------------------
static inline cd muli(cd z) { return cd(-z.imag(), z.real()); }

static void dif4(vector<cd>& a, std::uint32_t n) {
    std::uint32_t m = (std::uint32_t)std::__lg(n);
    for (std::uint32_t q = n >> 2; q >= 1; q >>= 2) {
        const std::uint32_t span = q << 2, numGroups = n / span;
        for (std::uint32_t b = 0; b < numGroups; ++b) {
            const cd w = tw[2 * b], w2 = w * w, w3 = w2 * w;
            cd* g = a.data() + (size_t)b * span;
            for (std::uint32_t k = 0; k < q; ++k) {
                cd X = g[k], Y = g[k + q] * w, Z = g[k + 2 * q] * w2, T = g[k + 3 * q] * w3;
                cd e0 = X + Z, e1 = X - Z, o0 = Y + T, io1 = muli(Y - T);
                g[k] = e0 + o0;
                g[k + q] = e0 - o0;
                g[k + 2 * q] = e1 + io1;
                g[k + 3 * q] = e1 - io1;
            }
        }
        if (q == 1) break;
    }
    if (m & 1) {                                   // leftover blockSize == 1 stage
        cd e = a[0], o = a[1];
        a[0] = e + o; a[1] = e - o;
        std::uint32_t wi = 1;
        for (std::uint32_t base = 2; base != n; base += 2, ++wi) {
            cd w = tw[wi], ee = a[base], oo = a[base + 1] * w;
            a[base] = ee + oo; a[base + 1] = ee - oo;
        }
    }
}

static void dit4(vector<cd>& a, std::uint32_t n) {
    std::uint32_t m = (std::uint32_t)std::__lg(n);
    if (m & 1) {                                   // leftover blockSize == 1 stage first
        cd e = a[0], o = a[1];
        a[0] = e + o; a[1] = e - o;
        std::uint32_t wi = 1;
        for (std::uint32_t base = 2; base != n; base += 2, ++wi) {
            cd w = tw[wi], ee = a[base], oo = a[base + 1];
            a[base] = ee + oo; a[base + 1] = (ee - oo) * std::conj(w);
        }
    }
    const std::uint32_t qStart = (m & 1) ? 2u : 1u;
    for (std::uint32_t q = qStart; q <= (n >> 2); q <<= 2) {
        const std::uint32_t span = q << 2, numGroups = n / span;
        for (std::uint32_t b = 0; b < numGroups; ++b) {
            const cd w = tw[2 * b], w2 = w * w, w3 = w2 * w;
            cd* g = a.data() + (size_t)b * span;
            for (std::uint32_t k = 0; k < q; ++k) {
                cd A = g[k], B = g[k + q], C = g[k + 2 * q], D = g[k + 3 * q];
                cd s0 = A + B, s1 = A - B, s2 = C + D, i3 = muli(C - D);
                g[k] = s0 + s2;
                g[k + q] = (s1 - i3) * std::conj(w);
                g[k + 2 * q] = (s0 - s2) * std::conj(w2);
                g[k + 3 * q] = (s1 + i3) * std::conj(w3);
            }
        }
    }
}

int main() {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    bool allOk = true;
    for (std::uint32_t bits = 2; bits <= 14; ++bits) {
        std::uint32_t n = 1u << bits;
        buildTwiddle(n);
        vector<cd> x(n);
        for (std::uint32_t i = 0; i < n; ++i) x[i] = cd(dist(rng), dist(rng));

        vector<cd> f2 = x, f4 = x;
        dif2(f2, n); dif4(f4, n);
        double difErr = 0;
        for (std::uint32_t i = 0; i < n; ++i) difErr = std::max(difErr, std::abs(f2[i] - f4[i]));

        vector<cd> t2 = x, t4 = x;
        dit2(t2, n); dit4(t4, n);
        double ditErr = 0;
        for (std::uint32_t i = 0; i < n; ++i) ditErr = std::max(ditErr, std::abs(t2[i] - t4[i]));

        // round trip through the mixed pair, as the real code does
        vector<cd> r = x; dif4(r, n); dit4(r, n);
        double rtErr = 0;
        for (std::uint32_t i = 0; i < n; ++i) rtErr = std::max(rtErr, std::abs(r[i] - (double)n * x[i]));

        double tol = 1e-11 * n;
        bool ok = difErr < tol && ditErr < tol && rtErr < tol;
        allOk = allOk && ok;
        printf("n=%-6u m=%2u  difErr=%.2e  ditErr=%.2e  rtErr=%.2e  %s\n",
               n, bits, difErr, ditErr, rtErr, ok ? "OK" : "FAIL");
    }
    printf("\n%s\n", allOk ? "ALL PASS" : "SOME FAILED");
    return allOk ? 0 : 1;
}
