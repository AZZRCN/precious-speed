// 喵喵喵~ https://space.bilibili.com/620657947
// ============================================================================
// soa_prod_bench.cpp — 生产 radix-4 DIF 核的 AoS vs SoA 实测基准
//
// 目的: 把 393027_opt.cpp 的生产前向 FFT 核 (difRec/difFlat/bf2Fwd/bf2FwdOne
//       + 两级 twiddle 表) 原样 port 到 SoA (Fr/Fi 分离), 然后
//         (1) verify : 证明 SoA 输出与 AoS **逐位相同** (bitwise) -> port 正确
//         (2) aos/soa: 分别只跑一种布局, 供 callgrind 测指令数 (唯一真值)
//
// 关键点: 逐位相同是可达的, 因为 AoS 的 fmaddsub 与 SoA 的 mul+fmsub/fmadd
//         对每个元素做的是**同一组 FP 运算、同一舍入序**:
//           AoS lane0: fmaddsub(ylo,wb, mul(yhi,wsw)) -> yr*wr - (yi*wi)   [t 先舍入, 再单次舍入 FMA]
//           SoA      : t = yi*wi ; re = fmsub(yr,wr,t)                      [完全一致]
//           AoS lane1: -> yr*wi + (yi*wr) ; SoA: u = yi*wr ; im = fmadd(yr,wi,u)
//         向量宽度不同 (AoS 2 复数/reg, SoA 4 复数/reg) 不影响逐位性: 元素间无耦合。
//
// 用法: ./soa_prod_bench verify|aos|soa|none [logn]
// ============================================================================
#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <complex>

using u32 = uint32_t;
using u64 = uint64_t;

#define FFT_LEAF_LOG 8

// ============================== AoS (生产原样) ==============================
namespace aos {
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
static inline cpx twlo(u32 i, cpx hi) { return cmul(twbase[i & twHalfMask], hi); }
static inline cpx twhi(u32 i) { return twbase[twHalfSize | (i >> twHalfLog)]; }
static inline cpx mulI(cpx z) {
    return _mm_xor_pd(_mm_shuffle_pd(z, z, 1), _mm_set_pd(0.0, -0.0));
}
static inline __m256d cmul4(__m256d y, __m256d wb, __m256d wsw) {
    __m256d ylo = _mm256_unpacklo_pd(y, y), yhi = _mm256_unpackhi_pd(y, y);
    return _mm256_fmaddsub_pd(ylo, wb, _mm256_mul_pd(yhi, wsw));
}
#define BC4(w) _mm256_set_m128d((w), (w))

static inline void bfPlain(cpx* s, u32 bs) {
    cpx* e = s + bs; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, y));
    }
    if (p != e) { cpx x = *p, y = p[bs]; *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y); }
}
static inline void bfFwd(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wsw = _mm256_permute_pd(w256, 0x5);
    cpx* e = s + bs; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d ylo = _mm256_unpacklo_pd(y, y), yhi = _mm256_unpackhi_pd(y, y);
        __m256d ym = _mm256_fmaddsub_pd(ylo, w256, _mm256_mul_pd(yhi, wsw));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, ym));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, ym));
    }
    if (p != e) { cpx x = *p, y = cmul(p[bs], w); *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y); }
}
static inline void bf2Fwd(cpx* s, u32 q, cpx w0, cpx w1, cpx w2) {
    const u32 bs = q << 1;
    const __m256d W0 = BC4(w0), W0s = _mm256_permute_pd(W0, 0x5);
    const __m256d W1 = BC4(w1), W1s = _mm256_permute_pd(W1, 0x5);
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = s + q; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d t2 = cmul4(_mm256_loadu_pd((const double*)(p + bs)), W0, W0s);
        __m256d t3 = cmul4(_mm256_loadu_pd((const double*)(p + bs + q)), W0, W0s);
        __m256d a0 = _mm256_add_pd(x0, t2), a2 = _mm256_sub_pd(x0, t2);
        __m256d a1 = _mm256_add_pd(x1, t3), a3 = _mm256_sub_pd(x1, t3);
        __m256d u1 = cmul4(a1, W1, W1s), u3 = cmul4(a3, W2, W2s);
        _mm256_storeu_pd((double*)p, _mm256_add_pd(a0, u1));
        _mm256_storeu_pd((double*)(p + q), _mm256_sub_pd(a0, u1));
        _mm256_storeu_pd((double*)(p + bs), _mm256_add_pd(a2, u3));
        _mm256_storeu_pd((double*)(p + bs + q), _mm256_sub_pd(a2, u3));
    }
    if (p != e) {
        cpx x0 = p[0], x1 = p[q], t2 = cmul(p[bs], w0), t3 = cmul(p[bs + q], w0);
        cpx a0 = _mm_add_pd(x0, t2), a2 = _mm_sub_pd(x0, t2);
        cpx a1 = _mm_add_pd(x1, t3), a3 = _mm_sub_pd(x1, t3);
        cpx u1 = cmul(a1, w1), u3 = cmul(a3, w2);
        p[0] = _mm_add_pd(a0, u1); p[q] = _mm_sub_pd(a0, u1);
        p[bs] = _mm_add_pd(a2, u3); p[bs + q] = _mm_sub_pd(a2, u3);
    }
}
static inline void bf2FwdOne(cpx* s, u32 q, cpx w2) {
    const u32 bs = q << 1;
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = s + q; cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d t2 = _mm256_loadu_pd((const double*)(p + bs));
        __m256d t3 = _mm256_loadu_pd((const double*)(p + bs + q));
        __m256d a0 = _mm256_add_pd(x0, t2), a2 = _mm256_sub_pd(x0, t2);
        __m256d a1 = _mm256_add_pd(x1, t3), a3 = _mm256_sub_pd(x1, t3);
        __m256d u3 = cmul4(a3, W2, W2s);
        _mm256_storeu_pd((double*)p, _mm256_add_pd(a0, a1));
        _mm256_storeu_pd((double*)(p + q), _mm256_sub_pd(a0, a1));
        _mm256_storeu_pd((double*)(p + bs), _mm256_add_pd(a2, u3));
        _mm256_storeu_pd((double*)(p + bs + q), _mm256_sub_pd(a2, u3));
    }
    if (p != e) {
        cpx x0 = p[0], x1 = p[q], t2 = p[bs], t3 = p[bs + q];
        cpx a0 = _mm_add_pd(x0, t2), a2 = _mm_sub_pd(x0, t2);
        cpx a1 = _mm_add_pd(x1, t3), a3 = _mm_sub_pd(x1, t3);
        cpx u3 = cmul(a3, w2);
        p[0] = _mm_add_pd(a0, a1); p[q] = _mm_sub_pd(a0, a1);
        p[bs] = _mm_add_pd(a2, u3); p[bs + q] = _mm_sub_pd(a2, u3);
    }
}
static void difFlat(cpx* d, u32 n, u32 bb) {
    u32 bc = 1, bs = n >> 1, st = n;
    for (; bs >= 2; bc <<= 2, st = bs >> 1, bs >>= 2) {
        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        u32 j = 0; cpx* s = d;
        if (base == 0) { bf2FwdOne(s, q, twg(1)); j = 1, s += st; }
        if ((bc << 1) <= twHalfSize) {
            const cpx hiA = twhi(base), hiB = twhi(base2);
            for (; j != bc; ++j, s += st) {
                const cpx w1 = twlo(base2 + 2 * j, hiB);
                bf2Fwd(s, q, twlo(base + j, hiA), w1, mulI(w1));
            }
        } else {
            for (; j != bc; ++j, s += st) {
                const cpx w1 = twg(base2 + 2 * j);
                bf2Fwd(s, q, twg(base + j), w1, mulI(w1));
            }
        }
    }
    if (bs == 1) {
        const u32 base = bb * bc;
        u32 j = 0; cpx* s = d;
        if (base == 0) { bfPlain(s, 1); j = 1, s += 2; }
        if (bc <= twHalfSize) {
            const cpx hiA = twhi(base);
            for (; j != bc; ++j, s += 2) bfFwd(s, 1, twlo(base + j, hiA));
        } else {
            for (; j != bc; ++j, s += 2) bfFwd(s, 1, twg(base + j));
        }
    }
}
static void difRec(cpx* d, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { difFlat(d, n, bb); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    if (bb == 0) bf2FwdOne(d, q, twg(1));
    else { const cpx w1 = twg(bb << 1); bf2Fwd(d, q, twg(bb), w1, mulI(w1)); }
    difRec(d, q, b4);
    difRec(d + q, q, b4 | 1);
    difRec(d + 2 * q, q, b4 | 2);
    difRec(d + 3 * q, q, b4 | 3);
}
}  // namespace aos

// ================================ SoA (port) ================================
namespace soa {
// twiddle 复用 aos::twg (循环外, O(1)/组), 保证 twiddle 值逐位相同
static inline void tw_split(__m128d w, double& wr, double& wi) {
    wr = _mm_cvtsd_f64(w);
    wi = _mm_cvtsd_f64(_mm_unpackhi_pd(w, w));
}
// 4 复数/reg 的复乘: 与 AoS fmaddsub 逐位等价 (见文件头说明)
static inline void cmulS(__m256d yr, __m256d yi, __m256d Wr, __m256d Wi,
                         __m256d& orr, __m256d& oii) {
    __m256d t = _mm256_mul_pd(yi, Wi);
    orr = _mm256_fmsub_pd(yr, Wr, t);
    __m256d u = _mm256_mul_pd(yi, Wr);
    oii = _mm256_fmadd_pd(yr, Wi, u);
}
static inline void cmulS1(double yr, double yi, double wr, double wi, double& orr, double& oii) {
    double t = yi * wi; orr = std::fma(yr, wr, -t);
    double u = yi * wr; oii = std::fma(yr, wi, u);
}

static inline void bfPlainS(double* Fr, double* Fi, size_t o, u32 bs) {
    u32 j = 0;
    for (; j + 4 <= bs; j += 4) {
        __m256d xr = _mm256_loadu_pd(Fr + o + j), xi = _mm256_loadu_pd(Fi + o + j);
        __m256d yr = _mm256_loadu_pd(Fr + o + bs + j), yi = _mm256_loadu_pd(Fi + o + bs + j);
        _mm256_storeu_pd(Fr + o + j, _mm256_add_pd(xr, yr));
        _mm256_storeu_pd(Fi + o + j, _mm256_add_pd(xi, yi));
        _mm256_storeu_pd(Fr + o + bs + j, _mm256_sub_pd(xr, yr));
        _mm256_storeu_pd(Fi + o + bs + j, _mm256_sub_pd(xi, yi));
    }
    for (; j < bs; ++j) {
        double xr = Fr[o + j], xi = Fi[o + j], yr = Fr[o + bs + j], yi = Fi[o + bs + j];
        Fr[o + j] = xr + yr; Fi[o + j] = xi + yi;
        Fr[o + bs + j] = xr - yr; Fi[o + bs + j] = xi - yi;
    }
}
static inline void bfFwdS(double* Fr, double* Fi, size_t o, u32 bs, double wr, double wi) {
    const __m256d Wr = _mm256_set1_pd(wr), Wi = _mm256_set1_pd(wi);
    u32 j = 0;
    for (; j + 4 <= bs; j += 4) {
        __m256d xr = _mm256_loadu_pd(Fr + o + j), xi = _mm256_loadu_pd(Fi + o + j);
        __m256d yr = _mm256_loadu_pd(Fr + o + bs + j), yi = _mm256_loadu_pd(Fi + o + bs + j);
        __m256d mr, mi; cmulS(yr, yi, Wr, Wi, mr, mi);
        _mm256_storeu_pd(Fr + o + j, _mm256_add_pd(xr, mr));
        _mm256_storeu_pd(Fi + o + j, _mm256_add_pd(xi, mi));
        _mm256_storeu_pd(Fr + o + bs + j, _mm256_sub_pd(xr, mr));
        _mm256_storeu_pd(Fi + o + bs + j, _mm256_sub_pd(xi, mi));
    }
    for (; j < bs; ++j) {
        double xr = Fr[o + j], xi = Fi[o + j];
        double mr, mi; cmulS1(Fr[o + bs + j], Fi[o + bs + j], wr, wi, mr, mi);
        Fr[o + j] = xr + mr; Fi[o + j] = xi + mi;
        Fr[o + bs + j] = xr - mr; Fi[o + bs + j] = xi - mi;
    }
}
static inline void bf2FwdS(double* Fr, double* Fi, size_t o, u32 q,
                           double w0r, double w0i, double w1r, double w1i, double w2r, double w2i) {
    const u32 bs = q << 1;
    const __m256d W0r = _mm256_set1_pd(w0r), W0i = _mm256_set1_pd(w0i);
    const __m256d W1r = _mm256_set1_pd(w1r), W1i = _mm256_set1_pd(w1i);
    const __m256d W2r = _mm256_set1_pd(w2r), W2i = _mm256_set1_pd(w2i);
    // v-SoA2: 8 个基指针提到循环外 -> 访问退化成 [reg + j*8] 单一寻址模式, 消掉每次迭代的 lea
    double* p0r = Fr + o;           double* p0i = Fi + o;
    double* p1r = Fr + o + q;       double* p1i = Fi + o + q;
    double* p2r = Fr + o + bs;      double* p2i = Fi + o + bs;
    double* p3r = Fr + o + bs + q;  double* p3i = Fi + o + bs + q;
    u32 j = 0;
    for (; j + 4 <= q; j += 4) {
        __m256d x0r = _mm256_loadu_pd(p0r + j), x0i = _mm256_loadu_pd(p0i + j);
        __m256d x1r = _mm256_loadu_pd(p1r + j), x1i = _mm256_loadu_pd(p1i + j);
        __m256d y2r = _mm256_loadu_pd(p2r + j), y2i = _mm256_loadu_pd(p2i + j);
        __m256d y3r = _mm256_loadu_pd(p3r + j), y3i = _mm256_loadu_pd(p3i + j);
        __m256d t2r, t2i, t3r, t3i;
        cmulS(y2r, y2i, W0r, W0i, t2r, t2i);
        cmulS(y3r, y3i, W0r, W0i, t3r, t3i);
        __m256d a0r = _mm256_add_pd(x0r, t2r), a0i = _mm256_add_pd(x0i, t2i);
        __m256d a2r = _mm256_sub_pd(x0r, t2r), a2i = _mm256_sub_pd(x0i, t2i);
        __m256d a1r = _mm256_add_pd(x1r, t3r), a1i = _mm256_add_pd(x1i, t3i);
        __m256d a3r = _mm256_sub_pd(x1r, t3r), a3i = _mm256_sub_pd(x1i, t3i);
        __m256d u1r, u1i, u3r, u3i;
        cmulS(a1r, a1i, W1r, W1i, u1r, u1i);
        cmulS(a3r, a3i, W2r, W2i, u3r, u3i);
        _mm256_storeu_pd(p0r + j, _mm256_add_pd(a0r, u1r));
        _mm256_storeu_pd(p0i + j, _mm256_add_pd(a0i, u1i));
        _mm256_storeu_pd(p1r + j, _mm256_sub_pd(a0r, u1r));
        _mm256_storeu_pd(p1i + j, _mm256_sub_pd(a0i, u1i));
        _mm256_storeu_pd(p2r + j, _mm256_add_pd(a2r, u3r));
        _mm256_storeu_pd(p2i + j, _mm256_add_pd(a2i, u3i));
        _mm256_storeu_pd(p3r + j, _mm256_sub_pd(a2r, u3r));
        _mm256_storeu_pd(p3i + j, _mm256_sub_pd(a2i, u3i));
    }
    for (; j < q; ++j) {
        double x0r = Fr[o + j], x0i = Fi[o + j], x1r = Fr[o + q + j], x1i = Fi[o + q + j];
        double t2r, t2i, t3r, t3i;
        cmulS1(Fr[o + bs + j], Fi[o + bs + j], w0r, w0i, t2r, t2i);
        cmulS1(Fr[o + bs + q + j], Fi[o + bs + q + j], w0r, w0i, t3r, t3i);
        double a0r = x0r + t2r, a0i = x0i + t2i, a2r = x0r - t2r, a2i = x0i - t2i;
        double a1r = x1r + t3r, a1i = x1i + t3i, a3r = x1r - t3r, a3i = x1i - t3i;
        double u1r, u1i, u3r, u3i;
        cmulS1(a1r, a1i, w1r, w1i, u1r, u1i);
        cmulS1(a3r, a3i, w2r, w2i, u3r, u3i);
        Fr[o + j] = a0r + u1r;          Fi[o + j] = a0i + u1i;
        Fr[o + q + j] = a0r - u1r;      Fi[o + q + j] = a0i - u1i;
        Fr[o + bs + j] = a2r + u3r;     Fi[o + bs + j] = a2i + u3i;
        Fr[o + bs + q + j] = a2r - u3r; Fi[o + bs + q + j] = a2i - u3i;
    }
}
static inline void bf2FwdOneS(double* Fr, double* Fi, size_t o, u32 q, double w2r, double w2i) {
    const u32 bs = q << 1;
    const __m256d W2r = _mm256_set1_pd(w2r), W2i = _mm256_set1_pd(w2i);
    double* p0r = Fr + o;           double* p0i = Fi + o;
    double* p1r = Fr + o + q;       double* p1i = Fi + o + q;
    double* p2r = Fr + o + bs;      double* p2i = Fi + o + bs;
    double* p3r = Fr + o + bs + q;  double* p3i = Fi + o + bs + q;
    u32 j = 0;
    for (; j + 4 <= q; j += 4) {
        __m256d x0r = _mm256_loadu_pd(p0r + j), x0i = _mm256_loadu_pd(p0i + j);
        __m256d x1r = _mm256_loadu_pd(p1r + j), x1i = _mm256_loadu_pd(p1i + j);
        __m256d t2r = _mm256_loadu_pd(p2r + j), t2i = _mm256_loadu_pd(p2i + j);
        __m256d t3r = _mm256_loadu_pd(p3r + j), t3i = _mm256_loadu_pd(p3i + j);
        __m256d a0r = _mm256_add_pd(x0r, t2r), a0i = _mm256_add_pd(x0i, t2i);
        __m256d a2r = _mm256_sub_pd(x0r, t2r), a2i = _mm256_sub_pd(x0i, t2i);
        __m256d a1r = _mm256_add_pd(x1r, t3r), a1i = _mm256_add_pd(x1i, t3i);
        __m256d a3r = _mm256_sub_pd(x1r, t3r), a3i = _mm256_sub_pd(x1i, t3i);
        __m256d u3r, u3i; cmulS(a3r, a3i, W2r, W2i, u3r, u3i);
        _mm256_storeu_pd(p0r + j, _mm256_add_pd(a0r, a1r));
        _mm256_storeu_pd(p0i + j, _mm256_add_pd(a0i, a1i));
        _mm256_storeu_pd(p1r + j, _mm256_sub_pd(a0r, a1r));
        _mm256_storeu_pd(p1i + j, _mm256_sub_pd(a0i, a1i));
        _mm256_storeu_pd(p2r + j, _mm256_add_pd(a2r, u3r));
        _mm256_storeu_pd(p2i + j, _mm256_add_pd(a2i, u3i));
        _mm256_storeu_pd(p3r + j, _mm256_sub_pd(a2r, u3r));
        _mm256_storeu_pd(p3i + j, _mm256_sub_pd(a2i, u3i));
    }
    for (; j < q; ++j) {
        double x0r = Fr[o + j], x0i = Fi[o + j], x1r = Fr[o + q + j], x1i = Fi[o + q + j];
        double t2r = Fr[o + bs + j], t2i = Fi[o + bs + j];
        double t3r = Fr[o + bs + q + j], t3i = Fi[o + bs + q + j];
        double a0r = x0r + t2r, a0i = x0i + t2i, a2r = x0r - t2r, a2i = x0i - t2i;
        double a1r = x1r + t3r, a1i = x1i + t3i, a3r = x1r - t3r, a3i = x1i - t3i;
        double u3r, u3i; cmulS1(a3r, a3i, w2r, w2i, u3r, u3i);
        Fr[o + j] = a0r + a1r;          Fi[o + j] = a0i + a1i;
        Fr[o + q + j] = a0r - a1r;      Fi[o + q + j] = a0i - a1i;
        Fr[o + bs + j] = a2r + u3r;     Fi[o + bs + j] = a2i + u3i;
        Fr[o + bs + q + j] = a2r - u3r; Fi[o + bs + q + j] = a2i - u3i;
    }
}
static void difFlatS(double* Fr, double* Fi, size_t o, u32 n, u32 bb) {
    u32 bc = 1, bs = n >> 1, st = n;
    for (; bs >= 2; bc <<= 2, st = bs >> 1, bs >>= 2) {
        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        u32 j = 0; size_t s = o;
        if (base == 0) {
            double w2r, w2i; tw_split(aos::twg(1), w2r, w2i);   // 与 AoS 同参: bf2FwdOne(s,q,twg(1))
            bf2FwdOneS(Fr, Fi, s, q, w2r, w2i); j = 1, s += st;
        }
        if ((bc << 1) <= aos::twHalfSize) {
            const __m128d hiA = aos::twhi(base), hiB = aos::twhi(base2);
            for (; j != bc; ++j, s += st) {
                const __m128d w1 = aos::twlo(base2 + 2 * j, hiB);
                const __m128d w0 = aos::twlo(base + j, hiA);
                const __m128d w2 = aos::mulI(w1);
                double w0r, w0i, w1r, w1i, w2r, w2i;
                tw_split(w0, w0r, w0i); tw_split(w1, w1r, w1i); tw_split(w2, w2r, w2i);
                bf2FwdS(Fr, Fi, s, q, w0r, w0i, w1r, w1i, w2r, w2i);
            }
        } else {
            for (; j != bc; ++j, s += st) {
                const __m128d w1 = aos::twg(base2 + 2 * j);
                const __m128d w0 = aos::twg(base + j);
                const __m128d w2 = aos::mulI(w1);
                double w0r, w0i, w1r, w1i, w2r, w2i;
                tw_split(w0, w0r, w0i); tw_split(w1, w1r, w1i); tw_split(w2, w2r, w2i);
                bf2FwdS(Fr, Fi, s, q, w0r, w0i, w1r, w1i, w2r, w2i);
            }
        }
    }
    if (bs == 1) {
        const u32 base = bb * bc;
        u32 j = 0; size_t s = o;
        if (base == 0) { bfPlainS(Fr, Fi, s, 1); j = 1, s += 2; }
        if (bc <= aos::twHalfSize) {
            const __m128d hiA = aos::twhi(base);
            for (; j != bc; ++j, s += 2) {
                double wr, wi; tw_split(aos::twlo(base + j, hiA), wr, wi);
                bfFwdS(Fr, Fi, s, 1, wr, wi);
            }
        } else {
            for (; j != bc; ++j, s += 2) {
                double wr, wi; tw_split(aos::twg(base + j), wr, wi);
                bfFwdS(Fr, Fi, s, 1, wr, wi);
            }
        }
    }
}
static void difRecS(double* Fr, double* Fi, size_t o, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { difFlatS(Fr, Fi, o, n, bb); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    if (bb == 0) {
        double w2r, w2i; tw_split(aos::twg(1), w2r, w2i);
        bf2FwdOneS(Fr, Fi, o, q, w2r, w2i);
    } else {
        const __m128d w1 = aos::twg(bb << 1);
        const __m128d w0 = aos::twg(bb);
        const __m128d w2 = aos::mulI(w1);
        double w0r, w0i, w1r, w1i, w2r, w2i;
        tw_split(w0, w0r, w0i); tw_split(w1, w1r, w1i); tw_split(w2, w2r, w2i);
        bf2FwdS(Fr, Fi, o, q, w0r, w0i, w1r, w1i, w2r, w2i);
    }
    difRecS(Fr, Fi, o, q, b4);
    difRecS(Fr, Fi, o + q, q, b4 | 1);
    difRecS(Fr, Fi, o + 2 * q, q, b4 | 2);
    difRecS(Fr, Fi, o + 3 * q, q, b4 | 3);
}
}  // namespace soa

// ================================= harness ==================================
static u64 rs = 0x9E3779B97F4A7C15ull;
static inline u64 rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

int main(int argc, char** argv) {
    const char* mode = (argc > 1) ? argv[1] : "verify";
    const int logn = (argc > 2) ? atoi(argv[2]) : 20;
    const u32 n = 1u << logn;

    aos::cpx* d = (aos::cpx*)aligned_alloc(64, (size_t)n * sizeof(aos::cpx));
    double* Fr = (double*)aligned_alloc(64, (size_t)n * sizeof(double));
    double* Fi = (double*)aligned_alloc(64, (size_t)n * sizeof(double));
    double* Sr = (double*)aligned_alloc(64, (size_t)n * sizeof(double));
    double* Si = (double*)aligned_alloc(64, (size_t)n * sizeof(double));
    if (!d || !Fr || !Fi || !Sr || !Si) { printf("alloc fail\n"); return 1; }

    // 同一份随机输入, 两种布局
    for (u32 i = 0; i < n; ++i) {
        double re = (double)(int32_t)(rnd() & 0xFFFF) - 32768.0;
        double im = (double)(int32_t)(rnd() & 0xFFFF) - 32768.0;
        d[i] = _mm_set_pd(im, re);
        Sr[i] = re; Si[i] = im;
    }
    aos::resize(n);

    if (!strcmp(mode, "none")) { printf("none logn=%d\n", logn); return 0; }

    if (!strcmp(mode, "aos")) {
        aos::difRec(d, n, 0);
        double s = 0; for (u32 i = 0; i < n; ++i) s += _mm_cvtsd_f64(d[i]);
        printf("aos logn=%d chk=%.17g\n", logn, s);
        return 0;
    }
    if (!strcmp(mode, "soa")) {
        memcpy(Fr, Sr, (size_t)n * 8); memcpy(Fi, Si, (size_t)n * 8);
        soa::difRecS(Fr, Fi, 0, n, 0);
        double s = 0; for (u32 i = 0; i < n; ++i) s += Fr[i];
        printf("soa logn=%d chk=%.17g\n", logn, s);
        return 0;
    }

    // verify: 逐位比对
    aos::difRec(d, n, 0);
    memcpy(Fr, Sr, (size_t)n * 8); memcpy(Fi, Si, (size_t)n * 8);
    soa::difRecS(Fr, Fi, 0, n, 0);
    size_t bad = 0; double maxabs = 0;
    for (u32 i = 0; i < n; ++i) {
        double ar = _mm_cvtsd_f64(d[i]);
        double ai = _mm_cvtsd_f64(_mm_unpackhi_pd(d[i], d[i]));
        u64 b1, b2, b3, b4;
        memcpy(&b1, &ar, 8); memcpy(&b2, &Fr[i], 8);
        memcpy(&b3, &ai, 8); memcpy(&b4, &Fi[i], 8);
        if (b1 != b2 || b3 != b4) {
            if (bad < 5) printf("  MISMATCH i=%u aos=(%.17g,%.17g) soa=(%.17g,%.17g)\n", i, ar, ai, Fr[i], Fi[i]);
            ++bad;
        }
        double e1 = std::fabs(ar - Fr[i]), e2 = std::fabs(ai - Fi[i]);
        if (e1 > maxabs) maxabs = e1;
        if (e2 > maxabs) maxabs = e2;
    }
    printf("verify logn=%d n=%u  bitwise_mismatch=%zu  maxabsdiff=%.3e  -> %s\n",
           logn, n, bad, maxabs, bad == 0 ? "BITWISE IDENTICAL (port OK)" : "PORT BUG");
    return bad == 0 ? 0 : 1;
}
