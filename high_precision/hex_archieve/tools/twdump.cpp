// AZZRCN
// https://github.com/AZZRCN
// 最小复现: 抽出 v13 的 twiddle 表生成 + twg, 打印角度以确定指数约定。
#include <immintrin.h>
#include <complex>
#include <cmath>
#include <cstdio>
#include <cstdint>
using u32 = uint32_t;
using cpx = __m128d;

alignas(64) static __m128d twbase[1u << 12];
static u32 twHalfLog = 0, twHalfSize = 0, twHalfMask = 0, twN = 0;

static inline cpx cmul(cpx a, cpx b) {
    return _mm_fmaddsub_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static void resize(u32 n) {
    if (n == twN) return;
    twN = n;
    const u32 halfLog = (u32)(31 - __builtin_clz(n)) >> 1, halfSize = 1u << halfLog;
    twHalfLog = halfLog; twHalfSize = halfSize; twHalfMask = halfSize - 1;
    const double a0 = std::acos(-1.0) / halfSize, a1 = a0 / halfSize;
    for (u32 i = 0, j = (halfSize * 3) >> 1, p = 0; i != halfSize; p -= halfSize - (j >> __builtin_ctz(++i))) {
        int32_t sp = (int32_t)p;
        std::complex<double> f = std::polar(1.0, sp * a0), s = std::polar(1.0, sp * a1);
        twbase[i] = _mm_set_pd(f.imag(), f.real());
        twbase[i | halfSize] = _mm_set_pd(s.imag(), s.real());
    }
}
static inline cpx twg(u32 i) {
    return cmul(twbase[i & twHalfMask], twbase[twHalfSize | (i >> twHalfLog)]);
}
static u32 brev(u32 x, int bits) {
    u32 r = 0;
    for (int i = 0; i < bits; ++i) if (x >> i & 1) r |= 1u << (bits - 1 - i);
    return r;
}
static void dump(u32 n) {
    resize(n);
    const int bits = 31 - __builtin_clz(n);
    const double TWO_PI = 2.0 * std::acos(-1.0);
    printf("n=%u  (twg(j) 应 == exp(-2*pi*i*brev_n(2j)/n) ?)\n", n);
    for (u32 j = 0; j < n / 2 && j < 16; ++j) {
        double v[2];
        _mm_storeu_pd(v, twg(j));
        double ang = std::atan2(v[1], v[0]);          // 实际角度
        double kf = -ang / TWO_PI * (double)n;        // 若 t=exp(-2pi i k/n), 这就是 k
        if (kf < -0.5) kf += n;
        printf("  j=%-3u twg=(% .6f,% .6f)  k_meas=%8.4f   brev_n(2j)=%-3u  brev_n(2j)/2=%.1f\n",
               j, v[0], v[1], kf, brev(2 * j, bits), brev(2 * j, bits) / 2.0);
    }
}
int main() {
    dump(16);
    printf("\n");
    dump(64);
    return 0;
}
