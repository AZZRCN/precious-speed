#include <immintrin.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#include <cstring>
#include <complex>

using u32 = uint32_t;
using u64 = uint64_t;

#ifndef FFT_LEAF_LOG
// 285H (callgrind, 13 瓶颈组) 4..14 单调, LEAF=8 最优 (比旧默认 11 省 ~1.5%).
// basecase DFT 成本 ∝ N*leaf, 叶越小基例越省; 递归开销在 LEAF=8 仍未主导.
#define FFT_LEAF_LOG 8
#endif
static bool g_sr = false;                 // SR=1 -> 前向用 split-radix DIF drop-in (默认关, 不动稳定版)
static int  g_sr_rev = 0;                 // 位反转模式: 0=二进制(默认), 1=base-4 (SRCMP 决定)
namespace fft {
using cpx = __m128d;
// v10c: 两级 twiddle 表 (外提高位分量, 带跨块保护)。原 4 MiB 平表 -> 16 KiB base 表 (常驻 L1)。
// twg(i) = base[i & mask] * base[halfSize | (i >> halfLog)]  (原 resize 的构造式)
// 代价 1 复乘 (FP 端口仅 39%% 占用, 有富余); 收益 L2 miss -25%%。
alignas(64) static __m128d twbase[1u << 12];
static std::vector<cpx> twfull; static u32 twfullMask = 0;
static u32 twHalfLog = 0, twHalfSize = 0, twHalfMask = 0, twN = 0;

static inline cpx cmul(cpx a, cpx b) {
    return _mm_fmaddsub_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline cpx cmulconj(cpx a, cpx b) {  // a * conj(b)
    return _mm_fmsubadd_pd(_mm_unpacklo_pd(b, b), a, _mm_mul_pd(_mm_unpackhi_pd(b, b), _mm_permute_pd(a, 1)));
}
static inline cpx cmulspec(cpx a, cpx b) {
    return _mm_fmadd_pd(_mm_unpacklo_pd(a, a), b, _mm_mul_pd(_mm_unpackhi_pd(a, a), _mm_permute_pd(b, 1)));
}
static inline cpx cscale(cpx a, double s) { return _mm_mul_pd(a, _mm_set1_pd(s)); }

static void resize(u32 n) {  // n = 复数点数; 只建 16 KiB 的 base 表
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
    // 单级 twiddle 表: twfull[i] = 旧 twg(i) 精确值; 取用只需 1 load, 省 1 复乘 (Ir 指标下 cache miss 不计, 故单级更优)
    twfull.assign(n, _mm_set_pd(0.0, 1.0));
    for (u32 i = 0; i < n; ++i)
        twfull[i] = cmul(twbase[i & twHalfMask], twbase[twHalfSize | (i >> twHalfLog)]);
    twfullMask = n - 1;
}
// 两级查表: 1 次 L1 load(低位) + 1 次 L1 load(高位) + 1 复乘
static inline cpx twg(u32 i) {
    return twfull[i & twfullMask];
}
// 高位分量提到循环外时用: 低位 lo, 高位分量 hi 已备好
static inline cpx twlo(u32 i, cpx /*hi*/) { return twfull[i & twfullMask]; }
static inline cpx twhi(u32 i) { return twfull[i & twfullMask]; }
// tw[2i+1] == tw[2i] * i (dump 验证的恒等式) -> 省一次 load + 一次复乘
static inline cpx mulI(cpx z) {   // [re,im] -> [-im,re]
    return _mm_xor_pd(_mm_shuffle_pd(z, z, 1), _mm_set_pd(0.0, -0.0));
}

static inline void bfPlain(cpx* s, u32 bs) {
    cpx* e = s + bs;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, y));
    }
    if (p != e) {
        cpx x = *p, y = p[bs];
        *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y);
    }
}
static inline void bfFwd(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wsw = _mm256_permute_pd(w256, 0x5);
    cpx* e = s + bs;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d ylo = _mm256_unpacklo_pd(y, y), yhi = _mm256_unpackhi_pd(y, y);
        __m256d ym = _mm256_fmaddsub_pd(ylo, w256, _mm256_mul_pd(yhi, wsw));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, ym));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(x, ym));
    }
    if (p != e) {
        cpx x = *p, y = cmul(p[bs], w);
        *p = _mm_add_pd(x, y), p[bs] = _mm_sub_pd(x, y);
    }
}
static inline void bfInv(cpx* s, u32 bs, cpx w) {
    const __m256d w256 = _mm256_set_m128d(w, w);
    const __m256d wlo = _mm256_unpacklo_pd(w256, w256), whi = _mm256_unpackhi_pd(w256, w256);
    cpx* e = s + bs;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x = _mm256_loadu_pd((const double*)p);
        __m256d y = _mm256_loadu_pd((const double*)(p + bs));
        __m256d d = _mm256_sub_pd(x, y);
        __m256d ds = _mm256_permute_pd(d, 0x5);
        __m256d o = _mm256_fmsubadd_pd(wlo, d, _mm256_mul_pd(whi, ds));
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
        _mm256_storeu_pd((double*)(p + bs), o);
    }
    if (p != e) {
        cpx x = *p, y = p[bs];
        *p = _mm_add_pd(x, y), p[bs] = cmulconj(_mm_sub_pd(x, y), w);
    }
}

// ---- radix-2^2: 两层融合, 中间结果留寄存器, load/store 往返减半 ----
static inline __m256d cmul4(__m256d y, __m256d wb, __m256d wsw) {  // y * w
    __m256d ylo = _mm256_unpacklo_pd(y, y), yhi = _mm256_unpackhi_pd(y, y);
    return _mm256_fmaddsub_pd(ylo, wb, _mm256_mul_pd(yhi, wsw));
}
static inline __m256d cmulv(__m256d a, __m256d b) {  // 逐 lane 复数乘 (2 组)
    return _mm256_fmaddsub_pd(_mm256_unpacklo_pd(a, a), b,
                              _mm256_mul_pd(_mm256_unpackhi_pd(a, a), _mm256_permute_pd(b, 0x5)));
}
static inline __m256d cmulconj4(__m256d y, __m256d wlo, __m256d whi) {  // y * conj(w)
    return _mm256_fmsubadd_pd(wlo, y, _mm256_mul_pd(whi, _mm256_permute_pd(y, 0x5)));
}
// 逐 lane 的 a*conj(b) (b 是向量而非广播标量; 供混合 radix 顶层蝶形使用)
static inline __m256d cmulconjv(__m256d a, __m256d b) {
    return _mm256_fmsubadd_pd(_mm256_unpacklo_pd(b, b), a,
                              _mm256_mul_pd(_mm256_unpackhi_pd(b, b), _mm256_permute_pd(a, 0x5)));
}
static inline __m256d mulI4(__m256d z) {   // [re,im] -> [-im,re], 2 组
    return _mm256_xor_pd(_mm256_permute_pd(z, 0x5), _mm256_set_pd(0.0, -0.0, 0.0, -0.0));
}
#define BC4(w) _mm256_set_m128d((w), (w))

// DIF: 层1 系数 w0 (跨度 bs=2q), 层2 左半 w1 / 右半 w2 (跨度 q)
static inline void bf2Fwd(cpx* s, u32 q, cpx w0, cpx w1, cpx w2) {
    const u32 bs = q << 1;
    const __m256d W0 = BC4(w0), W0s = _mm256_permute_pd(W0, 0x5);
    const __m256d W1 = BC4(w1), W1s = _mm256_permute_pd(W1, 0x5);
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = s + q;
    cpx* p = s;
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
    if (p != e) {  // q == 1
        cpx x0 = p[0], x1 = p[q], t2 = cmul(p[bs], w0), t3 = cmul(p[bs + q], w0);
        cpx a0 = _mm_add_pd(x0, t2), a2 = _mm_sub_pd(x0, t2);
        cpx a1 = _mm_add_pd(x1, t3), a3 = _mm_sub_pd(x1, t3);
        cpx u1 = cmul(a1, w1), u3 = cmul(a3, w2);
        p[0] = _mm_add_pd(a0, u1); p[q] = _mm_sub_pd(a0, u1);
        p[bs] = _mm_add_pd(a2, u3); p[bs + q] = _mm_sub_pd(a2, u3);
    }
}
static inline void bf2FwdOne(cpx* s, u32 q, cpx w2) {  // w0 == w1 == 1
    const u32 bs = q << 1;
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = s + q;
    cpx* p = s;
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
// DIT: 内层 (跨度 q) 系数 w1/w2, 外层 (跨度 2q) 系数 w0; 全部乘 conj
static inline void bf2Inv(cpx* s, u32 q, cpx w0, cpx w1, cpx w2) {
    const u32 bs = q << 1;
    const __m256d W0 = BC4(w0), W0l = _mm256_unpacklo_pd(W0, W0), W0h = _mm256_unpackhi_pd(W0, W0);
    const __m256d W1 = BC4(w1), W1l = _mm256_unpacklo_pd(W1, W1), W1h = _mm256_unpackhi_pd(W1, W1);
    const __m256d W2 = BC4(w2), W2l = _mm256_unpacklo_pd(W2, W2), W2h = _mm256_unpackhi_pd(W2, W2);
    cpx* e = s + q;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d x2 = _mm256_loadu_pd((const double*)(p + bs));
        __m256d x3 = _mm256_loadu_pd((const double*)(p + bs + q));
        __m256d a0 = _mm256_add_pd(x0, x1), a1 = cmulconj4(_mm256_sub_pd(x0, x1), W1l, W1h);
        __m256d a2 = _mm256_add_pd(x2, x3), a3 = cmulconj4(_mm256_sub_pd(x2, x3), W2l, W2h);
        _mm256_storeu_pd((double*)p, _mm256_add_pd(a0, a2));
        _mm256_storeu_pd((double*)(p + bs), cmulconj4(_mm256_sub_pd(a0, a2), W0l, W0h));
        _mm256_storeu_pd((double*)(p + q), _mm256_add_pd(a1, a3));
        _mm256_storeu_pd((double*)(p + bs + q), cmulconj4(_mm256_sub_pd(a1, a3), W0l, W0h));
    }
    if (p != e) {
        cpx x0 = p[0], x1 = p[q], x2 = p[bs], x3 = p[bs + q];
        cpx a0 = _mm_add_pd(x0, x1), a1 = cmulconj(_mm_sub_pd(x0, x1), w1);
        cpx a2 = _mm_add_pd(x2, x3), a3 = cmulconj(_mm_sub_pd(x2, x3), w2);
        p[0] = _mm_add_pd(a0, a2); p[bs] = cmulconj(_mm_sub_pd(a0, a2), w0);
        p[q] = _mm_add_pd(a1, a3); p[bs + q] = cmulconj(_mm_sub_pd(a1, a3), w0);
    }
}
static inline void bf2InvOne(cpx* s, u32 q, cpx w2) {  // w0 == w1 == 1
    const u32 bs = q << 1;
    const __m256d W2 = BC4(w2), W2l = _mm256_unpacklo_pd(W2, W2), W2h = _mm256_unpackhi_pd(W2, W2);
    cpx* e = s + q;
    cpx* p = s;
    for (; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d x2 = _mm256_loadu_pd((const double*)(p + bs));
        __m256d x3 = _mm256_loadu_pd((const double*)(p + bs + q));
        __m256d a0 = _mm256_add_pd(x0, x1), a1 = _mm256_sub_pd(x0, x1);
        __m256d a2 = _mm256_add_pd(x2, x3), a3 = cmulconj4(_mm256_sub_pd(x2, x3), W2l, W2h);
        _mm256_storeu_pd((double*)p, _mm256_add_pd(a0, a2));
        _mm256_storeu_pd((double*)(p + bs), _mm256_sub_pd(a0, a2));
        _mm256_storeu_pd((double*)(p + q), _mm256_add_pd(a1, a3));
        _mm256_storeu_pd((double*)(p + bs + q), _mm256_sub_pd(a1, a3));
    }
    if (p != e) {
        cpx x0 = p[0], x1 = p[q], x2 = p[bs], x3 = p[bs + q];
        cpx a0 = _mm_add_pd(x0, x1), a1 = _mm_sub_pd(x0, x1);
        cpx a2 = _mm_add_pd(x2, x3), a3 = cmulconj(_mm_sub_pd(x2, x3), w2);
        p[0] = _mm_add_pd(a0, a2); p[bs] = _mm_sub_pd(a0, a2);
        p[q] = _mm_add_pd(a1, a3); p[bs + q] = _mm_sub_pd(a1, a3);
    }
}

static void difFlat(cpx* d, u32 n, u32 bb) {
    u32 bc = 1, bs = n >> 1, st = n;
    for (; bs >= 2; bc <<= 2, st = bs >> 1, bs >>= 2) {
        const u32 q = bs >> 1, base = bb * bc, base2 = base << 1;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2FwdOne(s, q, twg(1)); j = 1, s += st; }
        // base/base2 天然对齐 bc/2bc; 仅当整块不跨 halfSize 边界才可外提高位分量
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
    if (bs == 1) {  // 层数为奇数时剩最低一层
        const u32 base = bb * bc;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bfPlain(s, 1); j = 1, s += 2; }
        if (bc <= twHalfSize) {
            const cpx hiA = twhi(base);
            for (; j != bc; ++j, s += 2) bfFwd(s, 1, twlo(base + j, hiA));
        } else {
            for (; j != bc; ++j, s += 2) bfFwd(s, 1, twg(base + j));
        }
    }
}
static void ditFlat(cpx* d, u32 n, u32 bb) {
    u32 q = 1, bcOut = n >> 2;
    for (; (q << 2) <= n; q <<= 2, bcOut >>= 2) {
        const u32 st = q << 2, base = bb * bcOut, base2 = base << 1;
        u32 j = 0;
        cpx* s = d;
        if (base == 0) { bf2InvOne(s, q, twg(1)); j = 1, s += st; }
        if ((bcOut << 1) <= twHalfSize) {
            const cpx hiA = twhi(base), hiB = twhi(base2);
            for (; j != bcOut; ++j, s += st) {
                const cpx w1 = twlo(base2 + 2 * j, hiB);
                bf2Inv(s, q, twlo(base + j, hiA), w1, mulI(w1));
            }
        } else {
            for (; j != bcOut; ++j, s += st) {
                const cpx w1 = twg(base2 + 2 * j);
                bf2Inv(s, q, twg(base + j), w1, mulI(w1));
            }
        }
    }
    if ((q << 1) <= n) {  // 层数为奇数时剩最高一层 (bc == 1)
        const u32 base = bb;
        if (base == 0) bfPlain(d, q);
        else bfInv(d, q, twg(base));
    }
}
static void sr_dif(cpx* a, u32 n, int revmode);  // 前向声明 (定义见下方 SR 块, 同 namespace fft)
static void difRec(cpx* d, u32 n, u32 bb) {
    if (g_sr && bb == 0) { sr_dif(d, n, g_sr_rev); return; }
    if (n <= (1u << FFT_LEAF_LOG)) { difFlat(d, n, bb); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    if (bb == 0) bf2FwdOne(d, q, twg(1));
    else { const cpx w1 = twg(bb << 1); bf2Fwd(d, q, twg(bb), w1, mulI(w1)); }
    difRec(d, q, b4);
    difRec(d + q, q, b4 | 1);
    difRec(d + 2 * q, q, b4 | 2);
    difRec(d + 3 * q, q, b4 | 3);
}
// ---- shang ban (index >= n/2) quan ling shi de ding ceng radix-4 (bb == 0, w0=w1=1) ----
// t2 = t3 = 0  =>  a0 = a2 = x0, a1 = a3 = x1
//   p[0]=x0+x1  p[q]=x0-x1  p[2q]=x0+x1*w2  p[3q]=x0-x1*w2
// zhi du 2 ge quarter (sheng yi ban du liu liang), qie split wu xu qing ling shang ban.
static void difZeroHiTop(cpx* d, u32 n) {
    const u32 q = n >> 2, bs = q << 1;
    const cpx w2 = twg(1);
    const __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    cpx* e = d + q;
    cpx* p = d;
    for (; p + 2 <= e; p += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)p);
        __m256d x1 = _mm256_loadu_pd((const double*)(p + q));
        __m256d u3 = cmul4(x1, W2, W2s);
        _mm256_storeu_pd((double*)p, _mm256_add_pd(x0, x1));
        _mm256_storeu_pd((double*)(p + q), _mm256_sub_pd(x0, x1));
        _mm256_storeu_pd((double*)(p + bs), _mm256_add_pd(x0, u3));
        _mm256_storeu_pd((double*)(p + bs + q), _mm256_sub_pd(x0, u3));
    }
    if (p != e) {
        cpx x0 = p[0], x1 = p[q];
        cpx u3 = cmul(x1, w2);
        p[0] = _mm_add_pd(x0, x1); p[q] = _mm_sub_pd(x0, x1);
        p[bs] = _mm_add_pd(x0, u3); p[bs + q] = _mm_sub_pd(x0, u3);
    }
}
static void difRecZeroHi(cpx* d, u32 n) {
    const u32 q = n >> 2;
    difZeroHiTop(d, n);
    difRec(d, q, 0);
    difRec(d + q, q, 1);
    difRec(d + 2 * q, q, 2);
    difRec(d + 3 * q, q, 3);
}
// ===== SR-gated split-radix DIF drop-in (前向) =====
// 数学: sr_fft_r 是已证明正确的 DIT split-radix (natural 序 DFT, 同 sr_compat).
// 输出序: 由 g_sr_rev 选二进制/base-4 位反转, 写入 a, 以对接现有 folded pointwise + ditRec.
// 默认 g_sr=false, 不触发, 稳定版行为不变.
static cpx* g_sr_y = nullptr, * g_sr_t = nullptr, * g_sr_W = nullptr;
static u32 g_sr_cap = 0;
static void sr_ensure(u32 n) {
    if (n <= g_sr_cap) return;
    if (g_sr_y) free(g_sr_y); if (g_sr_t) free(g_sr_t); if (g_sr_W) free(g_sr_W);
    g_sr_y = (cpx*)malloc((size_t)n * sizeof(cpx));
    g_sr_t = (cpx*)malloc((size_t)n * sizeof(cpx));
    g_sr_W = (cpx*)malloc((size_t)n * sizeof(cpx));
    g_sr_cap = n;
}
static void sr_fillW(u32 n) {  // W[k] = exp(-2*pi*i*k/n), 前向
    static const double PI = 3.14159265358979323846;
    for (u32 k = 0; k < n; ++k) {
        double ang = -2.0 * PI * k / n;
        g_sr_W[k] = _mm_set_pd(std::sin(ang), std::cos(ang));
    }
}
// DIT split-radix, out-of-place: x 输入(natural), y 输出(natural), t 为单次预分配 scratch(无递归内分配)
// N = 全尺寸(用于旋转因子表 g_sr_W 索引缩放), n = 当前子变换尺寸
static void sr_fft_r(const cpx* x, cpx* y, u32 N, u32 n, cpx* t) {
    if (n == 1) { y[0] = x[0]; return; }
    if (n == 2) { cpx a0 = x[0], a1 = x[1]; y[0] = _mm_add_pd(a0, a1); y[1] = _mm_sub_pd(a0, a1); return; }
    u32 q = n >> 1, h = n >> 2;
    u32 str = N / n;  // W_n^k = g_sr_W[k*str]
    for (u32 k = 0; k < q; ++k) t[k] = x[2 * k];
    sr_fft_r(t, y, N, q, t + q);
    for (u32 k = 0; k < h; ++k) t[k] = x[4 * k + 1];
    sr_fft_r(t, y + q, N, h, t + h);
    for (u32 k = 0; k < h; ++k) t[k] = x[4 * k + 3];
    sr_fft_r(t, y + q + h, N, h, t + h);
    for (u32 k = 0; k < h; ++k) {
        cpx Wk = g_sr_W[k * str];          // W_N^k
        cpx W3k = g_sr_W[3 * k * str];     // W_N^{3k}
        cpx w1 = cmul(y[q + k], Wk);       // H1[k] * W_N^k
        cpx w3 = cmul(y[q + h + k], W3k);  // H3[k] * W_N^{3k}
        cpx t = _mm_add_pd(w1, w3);
        cpx u = _mm_sub_pd(w1, w3);
        cpx iu = mulI(u);                  // I * u
        cpx a0 = y[k], a1 = y[k + h];
        y[k]       = _mm_add_pd(a0, t);
        y[k + h]   = _mm_sub_pd(a1, iu);
        y[k + 2 * h] = _mm_sub_pd(a0, t);
        y[k + 3 * h] = _mm_add_pd(a1, iu);
    }
}
static u32 bitrev_u(u32 x, int bits) { u32 r = 0; for (int s = 0; s < bits; ++s) r |= ((x >> s) & 1) << (bits - 1 - s); return r; }
static u32 base4rev_u(u32 x, int bits) { u32 r = 0; for (int s = 0; s < bits; s += 2) r |= ((x >> s) & 3) << (bits - 2 - s); return r; }
static std::vector<u32> g_pi_cache[32];   // 按 bits 缓存 difRec 输出置换 pi
static const u32* get_pi(u32 n) {
    int bits = 31 - __builtin_clz(n);
    if (!g_pi_cache[bits].empty()) return g_pi_cache[bits].data();
    std::vector<cpx> X(n), D(n);
    for (u32 i = 0; i < n; ++i) X[i] = _mm_set_pd(0, 0);
    X[1] = _mm_set_pd(0, 1);                 // 位置 1 放实数 1 (冲激)
    std::copy(X.begin(), X.end(), D.begin()); // D 必须在 X[1] 设定后拷贝, 否则 D 全零
    bool sg = g_sr; g_sr = false;           // 临时关闭门控, 走原始 difRec 提取置换
    resize(n);
    difRec(D.data(), n, 0);                 // D[out] = exp(-2*pi*i*pi(out)/n)
    g_sr = sg;
    std::vector<u32> pi(n);
    const double PI = 3.14159265358979323846;
    for (u32 out = 0; out < n; ++out) {
        double re = ((double*)&D[out])[0], im = ((double*)&D[out])[1];
        double ang = std::atan2(im, re);     // = -2*pi*pi(out)/n
        long k = llround(-(long)n * ang / (2.0 * PI));
        k = ((k % (long)n) + (long)n) % (long)n;
        pi[out] = (u32)k;
    }
    g_pi_cache[bits] = std::move(pi);
    return g_pi_cache[bits].data();
}
// 启动时预计算所有尺寸的置换缓存, 避免处理期间 get_pi 调用 resize/difRec 破坏全局 twbase
// (newton_divide 嵌套 FFT 场景: get_pi 在嵌套 mulg 中被调用, resize 会改写外层变换的 twiddle 表)
static void sr_init_pi() {
    for (int bits = 1; bits <= 19; ++bits) (void)get_pi(1u << bits);
}
// sr_fft_r 算自然序 DFT 到 g_sr_y, 再用 pi(out) 重排成 difRec 输出序, 写入 a
static void sr_dif(cpx* a, u32 n, int /*revmode*/) {
    sr_ensure(n); sr_fillW(n);
    sr_fft_r((const cpx*)a, g_sr_y, n, n, g_sr_t);   // g_sr_y = 自然序 DFT
    const u32* pi = get_pi(n);
    for (u32 out = 0; out < n; ++out) {
        double* pa = (double*)&a[out]; const double* pb = (const double*)&g_sr_y[pi[out]];
        pa[0] = pb[0]; pa[1] = pb[1];
    }
}
static int sr_cmp_main() {
    // 多尺寸: sr_dif vs difRec (随机输入), 定位尺寸相关 bug
    for (int bits = 6; bits <= 17; ++bits) {
        u32 n = 1u << bits;
        resize(n);
        unsigned long long rng = 0x123456789ULL ^ ((unsigned long long)n * 2654435761ull);
        auto rnd = [&]()->double { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
            return (double)(rng >> 11) * (1.0 / (1ull << 53)) - 0.5; };
        std::vector<cpx> X(n), D(n), E(n);
        for (u32 i = 0; i < n; ++i) { double re = rnd(), im = rnd(); X[i] = _mm_set_pd(im, re); }
        bool sg = g_sr;
        g_sr = false; std::copy(X.begin(), X.end(), D.begin()); difRec(D.data(), n, 0);
        std::copy(X.begin(), X.end(), E.begin()); g_sr = true; sr_dif(E.data(), n, 0); g_sr = sg;
        double mx = 0;
        for (u32 i = 0; i < n; ++i) {
            double er = ((double*)&E[i])[0]-((double*)&D[i])[0], ei = ((double*)&E[i])[1]-((double*)&D[i])[1];
            mx = std::max(mx, std::sqrt(er*er + ei*ei));
        }
        printf("n=%7u  sr_dif vs difRec  maxdiff=%.3e\n", n, mx);
    }
    return 0;
}
static void ditRec(cpx* d, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { ditFlat(d, n, bb); return; }
    const u32 q = n >> 2, b4 = bb << 2;
    ditRec(d, q, b4);
    ditRec(d + q, q, b4 | 1);
    ditRec(d + 2 * q, q, b4 | 2);
    ditRec(d + 3 * q, q, b4 | 3);
    if (bb == 0) bf2InvOne(d, q, twg(1));
    else { const cpx w1 = twg(bb << 1); bf2Inv(d, q, twg(bb), w1, mulI(w1)); }
}

static void pointwise(cpx* F, cpx* G, u32 n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    F[0] = cscale(cmulspec(F[0], G[0]), nf);
    F[1] = cscale(cmul(F[1], G[1]), nf);
    const cpx cjm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), 0));
    const cpx ngm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), (int64_t)(1ull << 63)));
    // AVX2: 一次两组 (f, f+1) / (b, b-1)。低 lane 用 +tw, 高 lane 用 -tw。
    const __m256d CJ = _mm256_set_m128d(cjm, cjm);
    const __m256d NG = _mm256_set_m128d(ngm, _mm_setzero_pd());
    const __m256d SF = _mm256_set1_pd(sf);
    for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        u32 f = bs, b = f + bs - 1;
        for (; f + 2 <= be; f += 2, b -= 2) {
            __m256d Ff = _mm256_loadu_pd((const double*)(F + f));
            __m256d Gf = _mm256_loadu_pd((const double*)(G + f));
            __m256d Fb = _mm256_loadu_pd((const double*)(F + b - 1));  // [F[b-1], F[b]]
            __m256d Gb = _mm256_loadu_pd((const double*)(G + b - 1));
            Fb = _mm256_permute2f128_pd(Fb, Fb, 0x01);                 // [F[b], F[b-1]]
            Gb = _mm256_permute2f128_pd(Gb, Gb, 0x01);
            __m256d Fc = _mm256_xor_pd(Fb, CJ), Gc = _mm256_xor_pd(Gb, CJ);
            __m256d fe = _mm256_add_pd(Ff, Fc), fo = _mm256_sub_pd(Ff, Fc);
            __m256d ge = _mm256_add_pd(Gf, Gc), go = _mm256_sub_pd(Gf, Gc);
            const cpx t0 = twg(f >> 1);
            __m256d T = _mm256_xor_pd(_mm256_set_m128d(t0, t0), NG);
            __m256d pa = _mm256_sub_pd(cmulv(fe, ge), cmulv(cmulv(fo, go), T));
            __m256d pb = _mm256_add_pd(cmulv(ge, fo), cmulv(fe, go));
            __m256d rf = _mm256_mul_pd(_mm256_add_pd(pa, pb), SF);
            __m256d rb = _mm256_xor_pd(_mm256_mul_pd(_mm256_sub_pd(pa, pb), SF), CJ);
            _mm256_storeu_pd((double*)(F + f), rf);
            _mm256_storeu_pd((double*)(F + b - 1), _mm256_permute2f128_pd(rb, rb, 0x01));
        }
        for (; f != be; ++f, --b) {
            cpx Fc = _mm_xor_pd(F[b], cjm), Gc = _mm_xor_pd(G[b], cjm);
            cpx fe = _mm_add_pd(F[f], Fc), fo = _mm_sub_pd(F[f], Fc);
            cpx ge = _mm_add_pd(G[f], Gc), go = _mm_sub_pd(G[f], Gc);
            cpx t = (f & 1) ? _mm_xor_pd(twg(f >> 1), ngm) : twg(f >> 1);
            cpx pa = _mm_sub_pd(cmul(fe, ge), cmul(cmul(fo, go), t));
            cpx pb = _mm_add_pd(cmul(ge, fo), cmul(fe, go));
            F[f] = cscale(_mm_add_pd(pa, pb), sf);
            F[b] = _mm_xor_pd(cscale(_mm_sub_pd(pa, pb), sf), cjm);
        }
    }
}
static void pointwiseSq(cpx* F, u32 n) {
    const double nf = 1.0 / n, sf = nf * 0.25;
    F[0] = cscale(cmulspec(F[0], F[0]), nf);
    F[1] = cscale(cmul(F[1], F[1]), nf);
    const cpx cjm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), 0));
    const cpx ngm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), (int64_t)(1ull << 63)));
    for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
        for (u32 f = bs, b = f + bs - 1; f != be; ++f, --b) {
            cpx Fc = _mm_xor_pd(F[b], cjm);
            cpx fe = _mm_add_pd(F[f], Fc), fo = _mm_sub_pd(F[f], Fc);
            cpx t = (f & 1) ? _mm_xor_pd(twg(f >> 1), ngm) : twg(f >> 1);
            cpx pa = _mm_sub_pd(cmul(fe, fe), cmul(cmul(fo, fo), t));
            cpx eo = cmul(fe, fo);
            cpx pb = _mm_add_pd(eo, eo);
            F[f] = cscale(_mm_add_pd(pa, pb), sf);
            F[b] = _mm_xor_pd(cscale(_mm_sub_pd(pa, pb), sf), cjm);
        }
    }
}
    // ============ 混合 radix 顶层蝶形 (移植自 best/div_verifield 前人的实数FFT; 本路径走全复数) ============
    // 数学完全照搬前人 dif3Stage/idit3Stage/dif5Stage/idit5Stage; 旋转因子 base = -2pi/(3m) 或 -2pi/(5m),
    // 即标准复数 DFT 的 W_{3m}^c / W_{5m}^{jc} (前人 FFTTable3/5 的 FACTOR=1,2 / 1..4), 非实数半长约定.
    // 精度: fft_ceil_tiers 选的 lm' 恒 <= 原 2 幂 lm, 故 2^(2k)*lm <= 2^48 预算天然不破.
    static const double SQRT3_DIV2 = 0.866025403784438646763723170752936;
    static const double R5_C1 = 0.309016994374947424102293417183;  // cos(2pi/5)
    static const double R5_S1 = 0.951056516295153572116439333379;  // sin(2pi/5)
    static const double R5_C2 = -0.809016994374947424102293417183; // cos(4pi/5)
    static const double R5_S2 = 0.587785252292473129078558064009;  // sin(4pi/5)
    static inline cpx mkc(double re, double im) { return _mm_set_pd(im, re); }
    static inline double cre(cpx z) { return _mm_cvtsd_f64(z); }
    static inline double cim(cpx z) { return _mm_cvtsd_f64(_mm_unpackhi_pd(z, z)); }

    struct MR_Tw {                       // 顶层蝶形旋转因子: O(sqrt(m)) 两级小表 (同 v13 twbase/twg)
        std::vector<cpx> lo3, hi3;  size_t cached3 = 0;  u32 hl3 = 0, hmask3 = 0;
        std::vector<cpx> lo5, hi5;  size_t cached5 = 0;  u32 hl5 = 0, hmask5 = 0;
        static void build(size_t m, double rad, std::vector<cpx>& lo, std::vector<cpx>& hi,
                          u32& hl, u32& hmask) {
            const u32 bits = (u32)(63 - __builtin_clzll((unsigned long long)m));
            const u32 h = bits >> 1, H = 1u << h, NH = (u32)(m >> h);
            lo.resize(H); hi.resize(NH);
            for (u32 i = 0; i < H; ++i) { double a = rad * (double)i; lo[i] = mkc(std::cos(a), std::sin(a)); }
            for (u32 i = 0; i < NH; ++i) { double a = rad * (double)((size_t)i << h); hi[i] = mkc(std::cos(a), std::sin(a)); }
            hl = h; hmask = H - 1;
        }
        void ensure3(size_t m) {
            if (cached3 == m && !lo3.empty()) return;
            build(m, -3.14159265358979323846 * 2.0 / (3.0 * (double)m), lo3, hi3, hl3, hmask3);
            cached3 = m;
        }
        void ensure5(size_t m) {
            if (cached5 == m && !lo5.empty()) return;
            build(m, -3.14159265358979323846 * 2.0 / (5.0 * (double)m), lo5, hi5, hl5, hmask5);
            cached5 = m;
        }
    };
    static MR_Tw g_mr_tw;

    // radix-3 DIF 顶层: 总复数 ts=3m, 块 i 在 (cpx*)b0 + i*m. 数学照搬前人 dif3Stage.
    __attribute__((noinline)) static void dif3StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure3(m);
        const cpx* LO = g_mr_tw.lo3.data(); const cpx* HI = g_mr_tw.hi3.data();
        const u32 hl = g_mr_tw.hl3, H = g_mr_tw.hmask3 + 1;
        cpx* B1 = b0 + (size_t)m; cpx* B2 = b0 + 2*(size_t)m;
        const __m256d VH = _mm256_set1_pd(0.5), VS = _mm256_set1_pd(SQRT3_DIV2);
        for (u32 blk = 0; blk < m; blk += H) {
            const cpx wh = HI[blk >> hl];
            const __m256d wh4 = _mm256_set_m128d(wh, wh);
            for (u32 l = 0; l < H; l += 2) {
                const u32 c = blk + l;
                __m256d a0 = _mm256_loadu_pd((const double*)(b0 + c));
                __m256d a1 = _mm256_loadu_pd((const double*)(B1 + c));
                __m256d a2 = _mm256_loadu_pd((const double*)(B2 + c));
                __m256d s = _mm256_add_pd(a1, a2), d = _mm256_sub_pd(a1, a2);
                __m256d h = _mm256_fnmadd_pd(s, VH, a0);
                __m256d g = _mm256_mul_pd(mulI4(d), VS);
                __m256d w1 = cmulv(_mm256_loadu_pd((const double*)(LO + l)), wh4);
                __m256d w2 = cmulv(w1, w1);
                _mm256_storeu_pd((double*)(b0 + c), _mm256_add_pd(a0, s));
                _mm256_storeu_pd((double*)(B1 + c), cmulv(_mm256_sub_pd(h, g), w1));
                _mm256_storeu_pd((double*)(B2 + c), cmulv(_mm256_add_pd(h, g), w2));
            }
        }
    }
    __attribute__((noinline)) static void idit3StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure3(m);
        const cpx* LO = g_mr_tw.lo3.data(); const cpx* HI = g_mr_tw.hi3.data();
        const u32 hl = g_mr_tw.hl3, H = g_mr_tw.hmask3 + 1;
        cpx* B1 = b0 + (size_t)m; cpx* B2 = b0 + 2*(size_t)m;
        const __m256d VH = _mm256_set1_pd(0.5), VS = _mm256_set1_pd(SQRT3_DIV2);
        for (u32 blk = 0; blk < m; blk += H) {
            const cpx wh = HI[blk >> hl];
            const __m256d wh4 = _mm256_set_m128d(wh, wh);
            for (u32 l = 0; l < H; l += 2) {
                const u32 c = blk + l;
                __m256d t0 = _mm256_loadu_pd((const double*)(b0 + c));
                __m256d y1 = _mm256_loadu_pd((const double*)(B1 + c));
                __m256d y2 = _mm256_loadu_pd((const double*)(B2 + c));
                __m256d w1 = cmulv(_mm256_loadu_pd((const double*)(LO + l)), wh4);
                __m256d w2 = cmulv(w1, w1);
                __m256d x1 = cmulconjv(y1, w1), x2 = cmulconjv(y2, w2);
                __m256d s = _mm256_add_pd(x1, x2), d = _mm256_sub_pd(x1, x2);
                __m256d h = _mm256_fnmadd_pd(s, VH, t0);
                __m256d g = _mm256_mul_pd(mulI4(d), VS);
                _mm256_storeu_pd((double*)(b0 + c), _mm256_add_pd(t0, s));
                _mm256_storeu_pd((double*)(B1 + c), _mm256_add_pd(h, g));
                _mm256_storeu_pd((double*)(B2 + c), _mm256_sub_pd(h, g));
            }
        }
    }
    // radix-5 DIF 顶层: 总复数 ts=5m, 块 i 在 (cpx*)b0 + i*m. 数学照搬前人 dif5Stage.
    __attribute__((noinline)) static void dif5StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure5(m);
        const cpx* LO = g_mr_tw.lo5.data(); const cpx* HI = g_mr_tw.hi5.data();
        const u32 hl = g_mr_tw.hl5, H = g_mr_tw.hmask5 + 1;
        cpx* B1=b0+(size_t)m; cpx* B2=b0+2*(size_t)m; cpx* B3=b0+3*(size_t)m; cpx* B4=b0+4*(size_t)m;
        const __m256d C1=_mm256_set1_pd(R5_C1), S1=_mm256_set1_pd(R5_S1);
        const __m256d C2=_mm256_set1_pd(R5_C2), S2=_mm256_set1_pd(R5_S2);
        for (u32 blk = 0; blk < m; blk += H) {
            const cpx wh = HI[blk >> hl];
            const __m256d wh4 = _mm256_set_m128d(wh, wh);
            for (u32 l = 0; l < H; l += 2) {
                const u32 c = blk + l;
                __m256d a0=_mm256_loadu_pd((const double*)(b0+c)), a1=_mm256_loadu_pd((const double*)(B1+c));
                __m256d a2=_mm256_loadu_pd((const double*)(B2+c)), a3=_mm256_loadu_pd((const double*)(B3+c));
                __m256d a4=_mm256_loadu_pd((const double*)(B4+c));
                __m256d t1=_mm256_add_pd(a1,a4), t2=_mm256_add_pd(a2,a3);
                __m256d t3=_mm256_sub_pd(a1,a4), t4=_mm256_sub_pd(a2,a3);
                __m256d u1=_mm256_fmadd_pd(t2,C2,_mm256_fmadd_pd(t1,C1,a0));
                __m256d u2=_mm256_fmadd_pd(t2,C1,_mm256_fmadd_pd(t1,C2,a0));
                __m256d v1=mulI4(_mm256_fmadd_pd(t4,S2,_mm256_mul_pd(t3,S1)));
                __m256d v2=mulI4(_mm256_fnmadd_pd(t4,S1,_mm256_mul_pd(t3,S2)));
                __m256d w1=cmulv(_mm256_loadu_pd((const double*)(LO+l)), wh4);
                __m256d w2=cmulv(w1,w1), w3=cmulv(w2,w1), w4=cmulv(w2,w2);
                _mm256_storeu_pd((double*)(b0+c), _mm256_add_pd(a0,_mm256_add_pd(t1,t2)));
                _mm256_storeu_pd((double*)(B1+c), cmulv(_mm256_sub_pd(u1,v1), w1));
                _mm256_storeu_pd((double*)(B2+c), cmulv(_mm256_sub_pd(u2,v2), w2));
                _mm256_storeu_pd((double*)(B3+c), cmulv(_mm256_add_pd(u2,v2), w3));
                _mm256_storeu_pd((double*)(B4+c), cmulv(_mm256_add_pd(u1,v1), w4));
            }
        }
    }
    __attribute__((noinline)) static void idit5StageR(cpx* b0, u32 m) {
        g_mr_tw.ensure5(m);
        const cpx* LO = g_mr_tw.lo5.data(); const cpx* HI = g_mr_tw.hi5.data();
        const u32 hl = g_mr_tw.hl5, H = g_mr_tw.hmask5 + 1;
        cpx* B1=b0+(size_t)m; cpx* B2=b0+2*(size_t)m; cpx* B3=b0+3*(size_t)m; cpx* B4=b0+4*(size_t)m;
        const __m256d C1=_mm256_set1_pd(R5_C1), S1=_mm256_set1_pd(R5_S1);
        const __m256d C2=_mm256_set1_pd(R5_C2), S2=_mm256_set1_pd(R5_S2);
        for (u32 blk = 0; blk < m; blk += H) {
            const cpx wh = HI[blk >> hl];
            const __m256d wh4 = _mm256_set_m128d(wh, wh);
            for (u32 l = 0; l < H; l += 2) {
                const u32 c = blk + l;
                __m256d w1=cmulv(_mm256_loadu_pd((const double*)(LO+l)), wh4);
                __m256d w2=cmulv(w1,w1), w3=cmulv(w2,w1), w4=cmulv(w2,w2);
                __m256d y0=_mm256_loadu_pd((const double*)(b0+c));
                __m256d x1=cmulconjv(_mm256_loadu_pd((const double*)(B1+c)), w1);
                __m256d x2=cmulconjv(_mm256_loadu_pd((const double*)(B2+c)), w2);
                __m256d x3=cmulconjv(_mm256_loadu_pd((const double*)(B3+c)), w3);
                __m256d x4=cmulconjv(_mm256_loadu_pd((const double*)(B4+c)), w4);
                __m256d t1=_mm256_add_pd(x1,x4), t2=_mm256_add_pd(x2,x3);
                __m256d t3=_mm256_sub_pd(x1,x4), t4=_mm256_sub_pd(x2,x3);
                __m256d u1=_mm256_fmadd_pd(t2,C2,_mm256_fmadd_pd(t1,C1,y0));
                __m256d u2=_mm256_fmadd_pd(t2,C1,_mm256_fmadd_pd(t1,C2,y0));
                __m256d v1=mulI4(_mm256_fmadd_pd(t4,S2,_mm256_mul_pd(t3,S1)));
                __m256d v2=mulI4(_mm256_fnmadd_pd(t4,S1,_mm256_mul_pd(t3,S2)));
                _mm256_storeu_pd((double*)(b0+c), _mm256_add_pd(y0,_mm256_add_pd(t1,t2)));
                _mm256_storeu_pd((double*)(B1+c), _mm256_add_pd(u1,v1));
                _mm256_storeu_pd((double*)(B2+c), _mm256_add_pd(u2,v2));
                _mm256_storeu_pd((double*)(B3+c), _mm256_sub_pd(u2,v2));
                _mm256_storeu_pd((double*)(B4+c), _mm256_sub_pd(u1,v1));
            }
        }
    }
    // ---- 混合 radix 的实数频域点乘 (照搬前人 real_dot_binrev3/5 的块结构) ----
    // 布局: 总复数 ts = R*m, 块 j 的位置 p 承载频点 phi = R*P(p) + j, 其中
    //   P(p) = (m - brev_m(p)) mod m   <-- difRec 是共轭方向 DFT, 输出排列不是裸 brev.
    //   (实测反解: m=16 时 P = 0,8,12,4,14,6,10,2,15,7,11,3,13,5,9,1, 与该式逐项吻合)
    // 共轭伙伴 ts-phi = R*(m-1-P(p)) + (R-j):
    //   j=0 时伙伴仍在块 0, 且 P(f)+P(b)=m <=> b=f+bs-1, 正是 v13 pointwise 的配对形状 -> 直接复用;
    //   j>=1 时伙伴在块 R-j, 位置 q(p) 满足 P(q)=m-1-P(p) <=> q(p) = brev_m((1-brev_m(p)) mod m).
    // 旋转因子 (与 v13 同一约定; 实测 twg(i) = exp(+2pi i * brev_n(2i)/n)):
    //   t(phi) = exp(-2pi i * phi/ts) = tblk(p) * rho_j,  tblk(p) = (p&1)? -twg(p>>1) : twg(p>>1),
    //   rho_j = exp(-2pi i * j/(R*m)).
    // 归一化: 混合路径 nf = 1/ts (不是 1/m), 故由外部传入.

    // 跨块共轭配对表 q(p) (按 m 缓存; O(m) 构建, 远轻于一次 FFT)
    struct MR_Q {
        std::vector<u32> q;
        std::vector<u32> rv;
        u32 cached = 0;
        const u32* get(u32 m) {
            if (cached == m && !q.empty()) return q.data();
            const u32 bits = (u32)(31 - __builtin_clz(m));
            rv.resize(m); q.resize(m);
            rv[0] = 0;
            for (u32 p = 1; p < m; ++p) rv[p] = (rv[p >> 1] >> 1) | ((p & 1u) << (bits - 1));
            for (u32 p = 0; p < m; ++p) q[p] = rv[(1u + m - rv[p]) & (m - 1)];
            cached = m;
            return q.data();
        }
    };
    static MR_Q g_mr_q;

    // 块内自配对 (= v13 pointwise 的形状, 只是 nf 外部给定)
    static void pointwise_blk(cpx* F, cpx* G, u32 n, double nf) {
        const double sf = nf * 0.25;
        F[0] = cscale(cmulspec(F[0], G[0]), nf);   // 频点 0 与 Nyquist 打包在一处
        F[1] = cscale(cmul(F[1], G[1]), nf);       // 频点 ts/2 自共轭
        const cpx cjm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), 0));
        const cpx ngm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), (int64_t)(1ull << 63)));
        // v27: 标量 SSE 循环 -> AVX2 双 lane 打包 (与 pointwise 同构, 数学不变)
        const __m256d CJ = _mm256_set_m128d(cjm, cjm);
        const __m256d NG = _mm256_set_m128d(ngm, _mm_setzero_pd());
        const __m256d SF = _mm256_set1_pd(sf);
        for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
            u32 f = bs, b = f + bs - 1;
            for (; f + 2 <= be; f += 2, b -= 2) {
                __m256d Ff = _mm256_loadu_pd((const double*)(F + f));
                __m256d Gf = _mm256_loadu_pd((const double*)(G + f));
                __m256d Fb = _mm256_loadu_pd((const double*)(F + b - 1));
                __m256d Gb = _mm256_loadu_pd((const double*)(G + b - 1));
                Fb = _mm256_permute2f128_pd(Fb, Fb, 0x01);
                Gb = _mm256_permute2f128_pd(Gb, Gb, 0x01);
                __m256d Fc = _mm256_xor_pd(Fb, CJ), Gc = _mm256_xor_pd(Gb, CJ);
                __m256d fe = _mm256_add_pd(Ff, Fc), fo = _mm256_sub_pd(Ff, Fc);
                __m256d ge = _mm256_add_pd(Gf, Gc), go = _mm256_sub_pd(Gf, Gc);
                const cpx t0 = twg(f >> 1);
                __m256d T = _mm256_xor_pd(_mm256_set_m128d(t0, t0), NG);
                __m256d pa = _mm256_sub_pd(cmulv(fe, ge), cmulv(cmulv(fo, go), T));
                __m256d pb = _mm256_add_pd(cmulv(ge, fo), cmulv(fe, go));
                __m256d rf = _mm256_mul_pd(_mm256_add_pd(pa, pb), SF);
                __m256d rb = _mm256_xor_pd(_mm256_mul_pd(_mm256_sub_pd(pa, pb), SF), CJ);
                _mm256_storeu_pd((double*)(F + f), rf);
                _mm256_storeu_pd((double*)(F + b - 1), _mm256_permute2f128_pd(rb, rb, 0x01));
            }
            for (; f != be; ++f, --b) {
                cpx Fc = _mm_xor_pd(F[b], cjm), Gc = _mm_xor_pd(G[b], cjm);
                cpx fe = _mm_add_pd(F[f], Fc), fo = _mm_sub_pd(F[f], Fc);
                cpx ge = _mm_add_pd(G[f], Gc), go = _mm_sub_pd(G[f], Gc);
                cpx t = (f & 1) ? _mm_xor_pd(twg(f >> 1), ngm) : twg(f >> 1);
                cpx pa = _mm_sub_pd(cmul(fe, ge), cmul(cmul(fo, go), t));
                cpx pb = _mm_add_pd(cmul(ge, fo), cmul(fe, go));
                F[f] = cscale(_mm_add_pd(pa, pb), sf);
                F[b] = _mm_xor_pd(cscale(_mm_sub_pd(pa, pb), sf), cjm);
            }
        }
    }
    // 跨块配对: A[p] (块 j) <-> B[q(p)] (块 R-j); t = 块0 序列 * rho.
    // q 是 [0,m) 上的双射, p 遍历 [0,m) 恰好把两块各 m 个元素各处理一次.
    static void pointwise_cross(cpx* FA, cpx* FB2, cpx* GA, cpx* GB2, u32 m, cpx rho, double nf) {
        const double sf = nf * 0.25;
        const u32* qt = g_mr_q.get(m);
        const cpx cjm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), 0));
        const cpx ngm = _mm_castsi128_pd(_mm_set_epi64x((int64_t)(1ull << 63), (int64_t)(1ull << 63)));
        for (u32 p = 0; p < m; ++p) {
            const u32 q = qt[p];
            cpx Fc = _mm_xor_pd(FB2[q], cjm), Gc = _mm_xor_pd(GB2[q], cjm);
            cpx fe = _mm_add_pd(FA[p], Fc), fo = _mm_sub_pd(FA[p], Fc);
            cpx ge = _mm_add_pd(GA[p], Gc), go = _mm_sub_pd(GA[p], Gc);
            cpx t0 = twg(p >> 1);
            cpx t = cmul((p & 1) ? _mm_xor_pd(t0, ngm) : t0, rho);
            cpx pa = _mm_sub_pd(cmul(fe, ge), cmul(cmul(fo, go), t));
            cpx pb = _mm_add_pd(cmul(ge, fo), cmul(fe, go));
            FA[p]  = cscale(_mm_add_pd(pa, pb), sf);
            FB2[q] = _mm_xor_pd(cscale(_mm_sub_pd(pa, pb), sf), cjm);
        }
    }
    // R = 3 或 5 的整段点乘 (F,G 可同指针 -> 平方)
    static void pointwise_mixed(cpx* F, cpx* G, u32 m, u32 R) {
        const double nf = 1.0 / (double)((size_t)R * m);
        pointwise_blk(F, G, m, nf);
        const double base = -3.14159265358979323846 * 2.0 / ((double)R * (double)m);  // -2pi/(R*m)
        for (u32 j = 1; j * 2 < R; ++j) {                    // R=3: j=1; R=5: j=1,2
            const double ang = base * (double)j;
            const cpx rho = mkc(std::cos(ang), std::sin(ang));
            pointwise_cross(F + (size_t)j * m, F + (size_t)(R - j) * m,
                            G + (size_t)j * m, G + (size_t)(R - j) * m, m, rho, nf);
        }
    }
}  // namespace fft

int main(int argc, char** argv){
    int N = (argc>1)? atoi(argv[1]) : 131072;
    int K = (argc>2)? atoi(argv[2]) : 20;
    std::mt19937_64 rng(12345);
    std::vector<double> buf((size_t)N*2);
    for(auto& x: buf) x = (double)(rng()&0xFFFF) - 0x8000;
    fft::resize(N);
    for(int it=0; it<K; ++it){
        fft::difRec((fft::cpx*)buf.data(), (unsigned)N, 0);
        fft::ditRec((fft::cpx*)buf.data(), (unsigned)N, 0);
    }
    double chk=0; for(size_t i=0;i<buf.size();++i) chk += buf[i];
    printf("done N=%d K=%d chk=%.3f\n", N, K, chk);
    return 0;
}