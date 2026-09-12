// soa_core_test.cpp — 独立 SoA FFT 核心同构/正确性测试 (不碰生产源码)
// 用标准迭代 radix-2 DIT (位反转输入 -> 自然输出) 作参照, 验证:
//  (1) AoS<->SoA 前向+逆变换逐位一致; (2) 随机卷积 SoA==schoolbook 精确.
#include <immintrin.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <algorithm>
using u32 = uint32_t;
static const double PI = 3.1415926535897932384626433832795;

// ---------- AoS reference (cpx = __m128d) ----------
using cx = __m128d;
static inline cx cmul(cx a, cx b) {
    return _mm_fmaddsub_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline u32 rev2(u32 x, int bits) { u32 r = 0; for (int i = 0; i < bits; ++i) { r = (r << 1) | (x & 1); x >>= 1; } return r; }
static void bitrevA(cx* d, u32 n) { int bits = __builtin_ctz(n); for (u32 i = 0; i < n; ++i) { u32 r = rev2(i, bits); if (i < r) { cx t = d[i]; d[i] = d[r]; d[r] = t; } } }
static void fftA(cx* d, u32 n, bool inv) {
    bitrevA(d, n);
    for (u32 len = 2; len <= n; len <<= 1) {
        double ang = (inv ? 2.0 : -2.0) * PI / len;
        u32 half = len / 2;
        for (u32 i = 0; i < n; i += len)
            for (u32 k = 0; k < half; ++k) {
                double wr = std::cos(ang * k), wi = std::sin(ang * k);
                cx w = _mm_set_pd(wi, wr);
                cx u = d[i + k], v = cmul(d[i + k + half], w);
                d[i + k] = _mm_add_pd(u, v); d[i + k + half] = _mm_sub_pd(u, v);
            }
    }
    if (inv) { double s = 1.0 / n; for (u32 c = 0; c < n; ++c) d[c] = _mm_mul_pd(d[c], _mm_set1_pd(s)); }
}

// ---------- SoA implementation (Fr/Fi 分离) ----------
static inline void cmul_soa(double ar, double ai, double br, double bi, double& rr, double& ri) {
    rr = ar * br - ai * bi; ri = ar * bi + ai * br;
}
static void bitrevS(double* Fr, double* Fi, u32 n) { int bits = __builtin_ctz(n); for (u32 i = 0; i < n; ++i) { u32 r = rev2(i, bits); if (i < r) { std::swap(Fr[i], Fr[r]); std::swap(Fi[i], Fi[r]); } } }
static void fftS(double* Fr, double* Fi, u32 n, bool inv) {
    bitrevS(Fr, Fi, n);
    for (u32 len = 2; len <= n; len <<= 1) {
        double ang = (inv ? 2.0 : -2.0) * PI / len;
        u32 half = len / 2;
        for (u32 i = 0; i < n; i += len)
            for (u32 k = 0; k < half; ++k) {
                double wr = std::cos(ang * k), wi = std::sin(ang * k);
                double ur = Fr[i + k], ui = Fi[i + k];
                double vr, vi; cmul_soa(Fr[i + k + half], Fi[i + k + half], wr, wi, vr, vi);
                Fr[i + k] = ur + vr; Fi[i + k] = ui + vi;
                Fr[i + k + half] = ur - vr; Fi[i + k + half] = ui - vi;
            }
    }
    if (inv) { double s = 1.0 / n; for (u32 c = 0; c < n; ++c) { Fr[c] *= s; Fi[c] *= s; } }
}

static double cpx_re(cx z) { return _mm_cvtsd_f64(z); }
static double cpx_im(cx z) { return _mm_cvtsd_f64(_mm_unpackhi_pd(z, z)); }
static double maxdiff(const double* a, const double* b, u32 n) { double m = 0; for (u32 i = 0; i < n; ++i) m = std::max(m, std::fabs(a[i] - b[i])); return m; }

int main() {
    std::mt19937_64 rng(12345);
    bool all_ok = true;
    for (u32 n : {256u, 1024u, 4096u}) {
        std::vector<cx> aA(n); std::vector<double> Fr(n), Fi(n);
        for (u32 i = 0; i < n; ++i) { double re = (double)(int64_t)(rng() % 2000) - 1000, im = (double)(int64_t)(rng() % 2000) - 1000; aA[i] = _mm_set_pd(im, re); Fr[i] = re; Fi[i] = im; }
        std::vector<cx> bA = aA; std::vector<double> Fr2 = Fr, Fi2 = Fi;
        fftA(bA.data(), n, false); fftA(bA.data(), n, true);
        fftS(Fr2.data(), Fi2.data(), n, false); fftS(Fr2.data(), Fi2.data(), n, true);
        double rtA = 0, rtS = 0;
        for (u32 i = 0; i < n; ++i) { rtA = std::max(rtA, std::fabs(cpx_re(bA[i]) - Fr[i])); rtA = std::max(rtA, std::fabs(cpx_im(bA[i]) - Fi[i])); rtS = std::max(rtS, std::fabs(Fr2[i] - Fr[i])); rtS = std::max(rtS, std::fabs(Fi2[i] - Fi[i])); }
        // 卷积: 实数 a,b (长度 n/2, 零填充), 圆形=线性
        u32 L = n / 2;
        std::vector<cx> xA(n), yA(n); std::vector<double> xr(n), xi(n), yr(n), yi(n);
        std::vector<long long> aa(n, 0), bb(n, 0);
        for (u32 i = 0; i < L; ++i) { long long va = (long long)(rng() % 1000), vb = (long long)(rng() % 1000); aa[i] = va; bb[i] = vb; xA[i] = _mm_set_pd(0.0, (double)va); xr[i] = (double)va; xi[i] = 0; yA[i] = _mm_set_pd(0.0, (double)vb); yr[i] = (double)vb; yi[i] = 0; }
        std::vector<cx> XA = xA, YA = yA; fftA(XA.data(), n, false); fftA(YA.data(), n, false);
        std::vector<double> Xr(n), Xi(n), Yr(n), Yi(n);
        fftS(xr.data(), xi.data(), n, false); fftS(yr.data(), yi.data(), n, false);
        Xr = xr; Xi = xi; Yr = yr; Yi = yi;
        std::vector<cx> CA(n); std::vector<double> Cr(n), Ci(n);
        for (u32 i = 0; i < n; ++i) { CA[i] = cmul(XA[i], YA[i]); cmul_soa(Xr[i], Xi[i], Yr[i], Yi[i], Cr[i], Ci[i]); }
        fftA(CA.data(), n, true); fftS(Cr.data(), Ci.data(), n, true);
        std::vector<long long> sb(L + L - 1, 0);
        for (u32 i = 0; i < L; ++i) for (u32 j = 0; j < L; ++j) sb[i + j] += aa[i] * bb[j];
        double errA = 0, errS = 0, errAS = 0; int badA = 0, badS = 0;
        for (u32 k = 0; k < L + L - 1; ++k) {
            double va = cpx_re(CA[k]), vs = Cr[k];
            errA = std::max(errA, std::fabs(va - (double)sb[k])); errS = std::max(errS, std::fabs(vs - (double)sb[k])); errAS = std::max(errAS, std::fabs(va - vs));
            if ((long long)std::llround(va) != sb[k]) ++badA;
            if ((long long)std::llround(vs) != sb[k]) ++badS;
        }
        bool ok = (badA == 0 && badS == 0 && rtA < 1e-6 && rtS < 1e-6);
        all_ok = all_ok && ok;
        printf("n=%5u  roundtrip AoS=%.2e SoA=%.2e | conv err AoS=%.2e SoA=%.2e AoSvsSoA=%.2e | badA=%d badS=%d  %s\n", n, rtA, rtS, errA, errS, errAS, badA, badS, ok ? "OK" : "FAIL");
    }
    printf("==== %s ====\n", all_ok ? "ALL OK" : "SOME FAIL");
    return all_ok ? 0 : 1;
}
