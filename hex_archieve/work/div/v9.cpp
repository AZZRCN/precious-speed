// AZZRCN
// https://github.com/AZZRCN
// ============================ LC 回执 ============================
// Submission #391747  2026/8/10 14:40:48  C++23  AC  61 ms  31.13 MiB
//   计分点 length_ratio_integer_00 = 61ms
//   注意: 本地预测的最长点是 _02 (实测仅 56ms) -> 本地 Ir 排序与 LC 时间排序错位,
//         "哪个点最长" 也必须以 LC 回执为准。
//   本地预测 maxCyc 211.8M ~ 59.8ms  ->  实测 61ms (偏差 +2%, 几乎完美)
//   >>> div 预测准 = 计算密集 + 分块复用好; 反证 mul 的 +51% 偏差纯属访存。
//   >>> div 内部调 mul 的 FFT, mul 的带宽优化会直接下放到 div。
//   逐点明细: archive/submissions/391747_div_v8_61ms_AC.md
// =================================================================
// HEX division v8  (A>=0, B>0, floor 除法, 输出 "q r")
//  tier1 : la,lb<=16      -> u64 除法      (覆盖 small/medium_00/r_nearly_zero_00)
//  tier2 : la<=32,lb<=16  -> u128/u64
//  tier3 : nb==1          -> divrem_1
//  tier4 : 小规模         -> Knuth Algorithm D
//  tier5 : 大规模         -> Burnikel-Ziegler 递归除法 (叶子 Knuth D, 乘法走 AVX2 FFT)
// 注意: 绝不使用 #pragma GCC optimize —— LC 会 CE。只允许 target。
#pragma GCC target("avx2,fma,bmi,bmi2,popcnt,lzcnt")

#include <cstdint>
#include <cstring>
#include <cmath>
#include <complex>
#include <immintrin.h>
#include <unistd.h>

using u32 = uint32_t;
using u64 = uint64_t;
using u128 = __uint128_t;

static constexpr int PAD = 128;
static constexpr int INCAP = 9 << 20;
static constexpr int OUTCAP = 10 << 20;
static constexpr int MAXC = 100010;          // 1.6M hex / 16
#ifndef BZ_CUTOFF
#define BZ_CUTOFF 64                         // BZ 叶子规模 (limbs)
#endif
#ifndef BZ_MIN
#define BZ_MIN (BZ_CUTOFF * 2 + 32)          // 低于此规模直接 Knuth D
#endif
#ifndef KD_QMAX
#define KD_QMAX 64                           // 商 limb 数 <= 此值时 Knuth D 完胜 BZ
#endif
#ifndef MULBF_MAX
#define MULBF_MAX 48                         // mulg: nb <= 此值走学校法
#endif
#ifndef BARRETT_T
#define BARRETT_T 2
#endif
#ifndef BARRETT_NMIN
#define BARRETT_NMIN 512                     // t==2 时块长 >= 此值才用 Barrett
#endif

alignas(64) static char inbuf_[PAD + INCAP + PAD];
alignas(64) static char outbuf[OUTCAP + 256];
static char* const inbuf = inbuf_ + PAD;
static u64 A[MAXC + 8], B[MAXC + 8];
static u64 Qout[MAXC + 8], Rout[MAXC + 8];
static u64 AN[2 * MAXC + 16], BN[MAXC + 8];  // Knuth D 归一化缓冲
static u64 AS[MAXC + 8192], BS[MAXC + 8192]; // BZ 顶层 shift 缓冲 (块长 n 可略大于 nb)
static u64 VB[MAXC + 8192];                  // Barrett 倒数 V 的低 n limbs (V = 2^64n + VB)
static u64 WORK[3200000];                    // BZ/Barrett 工作栈 (~25.6MB), 峰值约 16*n
static u64* wp = WORK;

static constexpr u32 LMMAX = 1u << 20;
alignas(64) static double FB[LMMAX], GB[LMMAX];

// ============================ hex I/O ============================
struct MaskTab {
    uint8_t m[17][16];
    constexpr MaskTab() : m{} {
        for (int L = 0; L <= 16; ++L)
            for (int i = 0; i < 16; ++i) m[L][i] = (i >= 16 - L) ? 0xFF : 0x00;
    }
};
alignas(64) static constexpr MaskTab MT{};

static inline __m128i a2n(__m128i v) {
    __m128i s0 = _mm_sub_epi8(v, _mm_set1_epi8('0'));
    __m128i sa = _mm_sub_epi8(_mm_or_si128(v, _mm_set1_epi8(0x20)), _mm_set1_epi8('a' - 10));
    __m128i gt = _mm_cmpgt_epi8(s0, _mm_set1_epi8(9));
    return _mm_blendv_epi8(s0, sa, gt);
}
static inline u64 hexfull(const char* end) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((u64)_mm_cvtsi128_si64(b));
}
static inline u64 hexpart(const char* end, int L) {
    __m128i nib = a2n(_mm_loadu_si128((const __m128i*)(end - 16)));
    nib = _mm_and_si128(nib, _mm_load_si128((const __m128i*)MT.m[L]));
    __m128i b = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    b = _mm_packus_epi16(b, b);
    return __builtin_bswap64((u64)_mm_cvtsi128_si64(b));
}
static inline void hex16_store(u64 val, char* out) {
    val = __builtin_bswap64(val);
    __m128i v = _mm_cvtsi64_si128((long long)val);
    __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
    __m128i lo = _mm_and_si128(v, _mm_set1_epi8(0x0F));
    __m128i nib = _mm_unpacklo_epi8(hi, lo);
    __m128i gt9 = _mm_cmpgt_epi8(nib, _mm_set1_epi8(9));
    __m128i asc = _mm_add_epi8(nib, _mm_set1_epi8('0'));
    __m128i alp = _mm_add_epi8(nib, _mm_set1_epi8('A' - 10));
    _mm_storeu_si128((__m128i*)out, _mm_or_si128(_mm_andnot_si128(gt9, asc), _mm_and_si128(gt9, alp)));
}
static inline int tok_len(const char* p) {
    const __m256i sp = _mm256_set1_epi8(' ');
    __m256i v = _mm256_loadu_si256((const __m256i*)p);
    u32 m = ~(u32)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
    if (m) return (int)_tzcnt_u32(m);
    int n = 32;
    for (;;) {
        v = _mm256_loadu_si256((const __m256i*)(p + n));
        m = ~(u32)_mm256_movemask_epi8(_mm256_cmpgt_epi8(v, sp));
        if (m) return n + (int)_tzcnt_u32(m);
        n += 32;
    }
}
static inline int parse_limbs(const char* s, int n, u64* out) {
    int nc = (n + 15) >> 4;
    const char* end = s + n;
    int full = nc - 1;
    for (int c = 0; c < full; ++c) out[c] = hexfull(end - (c << 4));
    int rem = n - (full << 4);
    out[full] = hexpart(s + rem, rem);
    return nc;
}
static inline char* put_u64(char* out, u64 v) {
    int d = v ? 16 - (int)(_lzcnt_u64(v) >> 2) : 1;
    hex16_store(v << (64 - 4 * d), out);
    return out + d;
}
static inline char* put_big(char* out, const u64* V, int n) {
    while (n > 1 && V[n - 1] == 0) --n;
    if (n <= 0 || (n == 1 && V[0] == 0)) { *out++ = '0'; return out; }
    u64 top = V[n - 1];
    int d = 16 - (int)(_lzcnt_u64(top) >> 2);
    hex16_store(top << (64 - 4 * d), out);
    out += d;
    for (int c = n - 2; c >= 0; --c) { hex16_store(V[c], out); out += 16; }
    return out;
}

// ============================ AVX2 FFT ============================
#define FFT_LEAF_LOG 10
namespace fft {
using cpx = __m128d;
// v10c: 两级 twiddle 表 (外提高位分量, 带跨块保护)。原 4 MiB 平表 -> 16 KiB base 表 (常驻 L1)。
// twg(i) = base[i & mask] * base[halfSize | (i >> halfLog)]  (原 resize 的构造式)
// 代价 1 复乘 (FP 端口仅 39%% 占用, 有富余); 收益 L2 miss -25%%。
alignas(64) static __m128d twbase[1u << 12];
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
}
// 两级查表: 1 次 L1 load(低位) + 1 次 L1 load(高位) + 1 复乘
static inline cpx twg(u32 i) {
    return cmul(twbase[i & twHalfMask], twbase[twHalfSize | (i >> twHalfLog)]);
}
// 高位分量提到循环外时用: 低位 lo, 高位分量 hi 已备好
static inline cpx twlo(u32 i, cpx hi) { return cmul(twbase[i & twHalfMask], hi); }
static inline cpx twhi(u32 i) { return twbase[twHalfSize | (i >> twHalfLog)]; }
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
}  // namespace fft

static void split_b2(const u64* src, double* g, size_t n, int k) {
    const u32* f = (const u32*)src;
    u64 tmp = f[0], msk = (u64(1) << k) - 1;
    size_t i = 1, j = 0;
    int w = 32;
    while (i < (n << 1)) {
        g[j++] = (double)(int64_t)(tmp & msk);
        tmp >>= k, w -= k;
        if (w < 32) { tmp |= u64(f[i++]) << w; w += 32; }
    }
    while (w > 0) { g[j++] = (double)(int64_t)(tmp & msk); tmp >>= k, w -= k; }
}
static void merge_b2(u64* f, const double* g, size_t n, int k) {
    size_t i = 0, j = 0;
    int w = 0;
    u128 tmp = 0;
    while (i < n) {
        while (w < 64) { tmp += (u128)(u64)(int64_t)(g[j++] + 0.5) << w; w += k; }
        f[i++] = (u64)tmp;
        tmp >>= 64, w -= 64;
    }
}
static inline int pick_k(size_t u) {
    static const size_t gk[11] = {19ull << 4,  18ull << 6,  17ull << 8,  16ull << 10,
                                  15ull << 12, 14ull << 14, 13ull << 16, 12ull << 18,
                                  11ull << 20, 10ull << 22, ~size_t(0)};
    int i = 0;
    while (u > gk[i]) ++i;
    return 19 - i;
}
static void mul_bf(const u64* a, int na, const u64* b, int nb, u64* c) {
    std::memset(c, 0, (size_t)(na + nb) * 8);
    for (int i = 0; i < nb; ++i) {
        u64 carry = 0, bi = b[i];
        for (int j = 0; j < na; ++j) {
            u128 t = (u128)a[j] * bi + c[i + j] + carry;
            c[i + j] = (u64)t;
            carry = (u64)(t >> 64);
        }
        c[i + na] = carry;
    }
}
static void mul_fft(const u64* a, int na, const u64* b, int nb, u64* c) {
    const size_t u = (size_t)na + nb;
    const int k = pick_k(u);
    const u32 coeffs = (u32)(u * 64 / k) + 1;
    const u32 lm = 2u << (31 - __builtin_clz(coeffs));
    std::memset(FB, 0, (size_t)lm * 8);
    std::memset(GB, 0, (size_t)lm * 8);
    split_b2(a, FB, na, k);
    split_b2(b, GB, nb, k);
    const u32 ts = lm >> 1;
    fft::resize(ts);
    fft::difRec((fft::cpx*)FB, ts, 0);
    fft::difRec((fft::cpx*)GB, ts, 0);
    fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
    fft::ditRec((fft::cpx*)FB, ts, 0);
    merge_b2(c, FB, u, k);
}
// c[0..na+nb) = a*b
static void mulg(const u64* a, int na, const u64* b, int nb, u64* c) {
    if (na < nb) { const u64* t = a; a = b; b = t; int s = na; na = nb; nb = s; }
    if (nb <= MULBF_MAX) mul_bf(a, na, b, nb, c);
    else mul_fft(a, na, b, nb, c);
}

// ---- 固定乘数的 FFT 复用: Barrett 块循环里 q2 / BS 恒定, 正变换只做一次 ----
static constexpr u32 FMCAP = 1u << 20;
struct FixedFFT { size_t u; u32 lm, ts; int k; bool ok; };
alignas(64) static double FMG1[FMCAP], FMG2[FMCAP];
static FixedFFT FF1, FF2;

static void fm_prep(FixedFFT& F, double* G, const u64* b, int nb, int na) {
    F.ok = false;
    if (nb <= MULBF_MAX) return;
    F.u = (size_t)na + nb;
    F.k = pick_k(F.u);
    const u32 coeffs = (u32)(F.u * 64 / F.k) + 1;
    F.lm = 2u << (31 - __builtin_clz(coeffs));
    if (F.lm > FMCAP) return;
    F.ts = F.lm >> 1;
    std::memset(G, 0, (size_t)F.lm * 8);
    split_b2(b, G, nb, F.k);
    fft::resize(F.ts);
    fft::difRec((fft::cpx*)G, F.ts, 0);
    F.ok = true;
}
static void fm_mul(const FixedFFT& F, double* G, const u64* a, int na, u64* c) {
    std::memset(FB, 0, (size_t)F.lm * 8);
    split_b2(a, FB, na, F.k);
    fft::resize(F.ts);
    fft::difRec((fft::cpx*)FB, F.ts, 0);
    fft::pointwise((fft::cpx*)FB, (fft::cpx*)G, F.ts);
    fft::ditRec((fft::cpx*)FB, F.ts, 0);
    merge_b2(c, FB, F.u, F.k);
}

// ============================ 基本 limb 运算 ============================
static inline int cmpn(const u64* a, const u64* b, int n) {
    for (int i = n - 1; i >= 0; --i)
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    return 0;
}
static inline u64 addn(u64* a, const u64* b, int n) {
    unsigned char c = 0;
    for (int i = 0; i < n; ++i) c = _addcarry_u64(c, a[i], b[i], (unsigned long long*)&a[i]);
    return c;
}
static inline u64 subn(u64* a, const u64* b, int n) {
    unsigned char c = 0;
    for (int i = 0; i < n; ++i) c = _subborrow_u64(c, a[i], b[i], (unsigned long long*)&a[i]);
    return c;
}

// ============================ Knuth D ============================
static void knuthD(const u64* U, int mn, const u64* V, int n, u64* Qo, u64* Ro) {
    const int m = mn - n;
    const int s = (int)_lzcnt_u64(V[n - 1]);
    if (s) {
        for (int i = n - 1; i > 0; --i) BN[i] = (V[i] << s) | (V[i - 1] >> (64 - s));
        BN[0] = V[0] << s;
        for (int i = mn - 1; i > 0; --i) AN[i] = (U[i] << s) | (U[i - 1] >> (64 - s));
        AN[0] = U[0] << s;
        AN[mn] = U[mn - 1] >> (64 - s);
    } else {
        std::memcpy(BN, V, (size_t)n * 8);
        std::memcpy(AN, U, (size_t)mn * 8);
        AN[mn] = 0;
    }
    const u64 vn1 = BN[n - 1], vn2 = BN[n - 2];
    for (int j = m; j >= 0; --j) {
        const u128 num = ((u128)AN[j + n] << 64) | AN[j + n - 1];
        u64 qhat, rhat = 0;
        bool refine = true;
        if (AN[j + n] >= vn1) {
            qhat = ~0ULL;
            u128 t = num - (u128)qhat * vn1;
            if (t >> 64) refine = false; else rhat = (u64)t;
        } else {
            qhat = (u64)(num / vn1);
            rhat = (u64)(num - (u128)qhat * vn1);
        }
        if (refine) {
            while ((u128)qhat * vn2 > (((u128)rhat << 64) | AN[j + n - 2])) {
                --qhat;
                rhat += vn1;
                if (rhat < vn1) break;
            }
        }
        u64 carry = 0, borrow = 0;
        for (int i = 0; i < n; ++i) {
            u128 pr = (u128)qhat * BN[i] + carry;
            carry = (u64)(pr >> 64);
            u64 sub = (u64)pr, cur = AN[i + j];
            u64 d1 = cur - sub;
            u64 nb = (cur < sub);
            u64 d2 = d1 - borrow;
            nb += (d1 < borrow);
            AN[i + j] = d2;
            borrow = nb;
        }
        {
            u64 cur = AN[j + n];
            u64 d1 = cur - carry;
            u64 nb = (cur < carry);
            u64 d2 = d1 - borrow;
            nb += (d1 < borrow);
            AN[j + n] = d2;
            borrow = nb;
        }
        if (borrow) {
            --qhat;
            unsigned char c = 0;
            for (int i = 0; i < n; ++i)
                c = _addcarry_u64(c, AN[i + j], BN[i], (unsigned long long*)&AN[i + j]);
            AN[j + n] += c;
        }
        Qo[j] = qhat;
    }
    if (s) {
        for (int i = 0; i < n - 1; ++i) Ro[i] = (AN[i] >> s) | (AN[i + 1] << (64 - s));
        Ro[n - 1] = AN[n - 1] >> s;
    } else {
        std::memcpy(Ro, AN, (size_t)n * 8);
    }
}

// ============================ Burnikel-Ziegler ============================
static void div_3n_2n(const u64* Ain, const u64* Bin, int n, u64* Q, u64* R);

// A: 2n limbs, B: n limbs (归一化: B[n-1] 最高位为 1), 且 A < B*2^(64n)
// Q: n limbs, R: n limbs
static void div_2n_1n(const u64* Ain, const u64* Bin, int n, u64* Q, u64* R) {
    if (n < BZ_CUTOFF || (n & 1)) {
        u64* Qt = wp; wp += n + 1;
        knuthD(Ain, 2 * n, Bin, n, Qt, R);
        std::memcpy(Q, Qt, (size_t)n * 8);
        wp = Qt;
        return;
    }
    const int n2 = n >> 1;
    u64* save = wp;
    u64* R1 = wp; wp += n;
    div_3n_2n(Ain + n2, Bin, n2, Q + n2, R1);      // 高 3*n2 limbs
    u64* T = wp; wp += 3 * n2;
    std::memcpy(T, Ain, (size_t)n2 * 8);
    std::memcpy(T + n2, R1, (size_t)n * 8);
    div_3n_2n(T, Bin, n2, Q, R);
    wp = save;
}

// A: 3n limbs, B: 2n limbs (归一化), 且 A < B*2^(64n)
// Q: n limbs, R: 2n limbs
static void div_3n_2n(const u64* Ain, const u64* Bin, int n, u64* Q, u64* R) {
    const u64* B1 = Bin + n;
    const u64* B0 = Bin;
    u64* save = wp;
    u64* R1 = wp; wp += n + 1;

    if (cmpn(Ain + 2 * n, B1, n) < 0) {
        div_2n_1n(Ain + n, B1, n, Q, R1);
        R1[n] = 0;
    } else {
        // A2 == B1  =>  Q = 2^(64n)-1,  R1 = A1 + B1
        for (int i = 0; i < n; ++i) Q[i] = ~0ULL;
        std::memcpy(R1, Ain + n, (size_t)n * 8);
        R1[n] = addn(R1, B1, n);
    }
    u64* D = wp; wp += 2 * n;
    mulg(Q, n, B0, n, D);
    u64* Rt = wp; wp += 2 * n + 1;
    std::memcpy(Rt, Ain, (size_t)n * 8);
    std::memcpy(Rt + n, R1, (size_t)(n + 1) * 8);
    u64 br = subn(Rt, D, 2 * n);
    Rt[2 * n] -= br;
    while ((int64_t)Rt[2 * n] < 0) {
        for (int i = 0; i < n; ++i) if (Q[i]--) break;
        u64 c = addn(Rt, Bin, 2 * n);
        Rt[2 * n] += c;
    }
    std::memcpy(R, Rt, (size_t)(2 * n) * 8);
    wp = save;
}

// ============================ Newton 倒数 (invertappr) ============================
// d: n limbs, 归一化 (bit 64n-1 = 1)。输出 v: n limbs。
// 保证 2^(64n) + v <= floor(2^(128n) / d)，且误差为 O(1)。
// 代价 ~2.5*M(n)，远低于 div_2n_1n 的 ~9*M(n)。
#ifndef INV_BASE
#define INV_BASE 48
#endif
static void invertappr(const u64* d, int n, u64* v) {
    if (n <= INV_BASE) {
        u64* U = wp; wp += 2 * n + 2;
        u64* Qt = wp; wp += n + 2;
        u64* Rt = wp; wp += n + 2;
        std::memset(U, 0xFF, (size_t)(2 * n) * 8);
        knuthD(U, 2 * n, d, n, Qt, Rt);          // 商 n+1 limbs, Qt[n]==1
        std::memcpy(v, Qt, (size_t)n * 8);
        wp = U;
        return;
    }
    const int h = (n >> 1) + 1;                  // 2h >= n+1: 保证误差平方后 < 1 limb
    const int l = n - h;
    u64* save = wp;
    u64* xh = wp; wp += h + 2;
    invertappr(d + l, h, xh);                    // xh ~ B^{2h}/d_hi - B^h  (下估)
    {                                            // xh -= 4  (保证 E >= 0)
        u64 bw = 4;
        for (int i = 0; i < h && bw; ++i) { u64 cur = xh[i]; xh[i] = cur - bw; bw = (cur < bw); }
        if (bw) std::memset(xh, 0, (size_t)h * 8);
    }
    // W = d * (B^n + xh*B^l)  (2n limbs, <= B^{2n})
    u64* T = wp; wp += n + h + 2;
    mulg(d, n, xh, h, T);
    u64* W = wp; wp += 2 * n + 2;
    std::memset(W, 0, (size_t)l * 8);
    std::memcpy(W + l, T, (size_t)(n + h) * 8);  // l + n + h == 2n
    {
        unsigned char c = 0;
        for (int i = 0; i < n; ++i)
            c = _addcarry_u64(c, W[n + i], d[i], (unsigned long long*)&W[n + i]);
    }
    // E = B^{2n} - W  (两补)
    {
        unsigned char c = 1;
        for (int i = 0; i < 2 * n; ++i)
            c = _addcarry_u64(c, ~W[i], 0ULL, (unsigned long long*)&W[i]);
    }
    const u64* Ehi = W + n;                      // floor(E / B^n), 有效 ~l+1 limbs
    int ne = n;
    while (ne > 0 && Ehi[ne - 1] == 0) --ne;
    // v = xh*B^l + Ehi + floor(Ehi*xh / B^h)
    std::memset(v, 0, (size_t)n * 8);
    std::memcpy(v + l, xh, (size_t)h * 8);
    if (ne > 0) {
        u64* P = wp; wp += ne + h + 2;
        mulg(Ehi, ne, xh, h, P);                 // ne+h limbs
        unsigned char c = 0, c2 = 0;
        const int lim = ne < n ? ne : n;
        for (int i = 0; i < lim; ++i) {
            u64 s;
            c = _addcarry_u64(c, Ehi[i], P[h + i], (unsigned long long*)&s);
            c2 = _addcarry_u64(c2, v[i], s, (unsigned long long*)&v[i]);
        }
        u64 hi = (u64)c;                          // Δ 的进位, 落在位置 lim
        for (int i = lim; i < n; ++i) {
            if (!(c2 | hi)) break;
            c2 = _addcarry_u64(c2, v[i], hi, (unsigned long long*)&v[i]);
            hi = 0;
        }
        if (c2 | hi) for (int i = 0; i < n; ++i) v[i] = ~0ULL;  // 仅 d == 2^(64n-1) 可达
        wp = P;
    }
    wp = save;
}

// 顶层 BZ: A(na) / B(nb) -> Q(na-nb+1), R(nb)。要求 na >= nb >= BZ_MIN。
static void bz_divide(const u64* Ai, int na, const u64* Bi, int nb, u64* Q, u64* R) {
    // 块长 n = j*m，m 为 2 的幂，保证递归能一路二分到 j
    int q = nb / BZ_CUTOFF;
    int m = 1 << (32 - __builtin_clz((u32)(q ? q : 1)));
    int j = (nb + m - 1) / m;
    int n = j * m;                       // n >= nb
    const int sigma = 64 * n - (64 * nb - (int)_lzcnt_u64(Bi[nb - 1]));
    const int sw = sigma >> 6, sb = sigma & 63;   // limb 位移 + 位内位移

    // BS = B << sigma  (n limbs, 最高位为 1)
    std::memset(BS, 0, (size_t)n * 8);
    if (sb) {
        u64 carry = 0;
        for (int i = 0; i < nb; ++i) { BS[sw + i] = (Bi[i] << sb) | carry; carry = Bi[i] >> (64 - sb); }
        if (sw + nb < n) BS[sw + nb] = carry;
    } else {
        std::memcpy(BS + sw, Bi, (size_t)nb * 8);
    }

    // AS = A << sigma  (nas limbs)
    int nas = na + sw + 1;
    std::memset(AS, 0, (size_t)nas * 8);
    if (sb) {
        u64 carry = 0;
        for (int i = 0; i < na; ++i) { AS[sw + i] = (Ai[i] << sb) | carry; carry = Ai[i] >> (64 - sb); }
        AS[sw + na] = carry;
    } else {
        std::memcpy(AS + sw, Ai, (size_t)na * 8);
    }
    while (nas > 1 && AS[nas - 1] == 0) --nas;

    // 块数 t: 按 bit 长度算, 多留 1 bit 保证最高块 a_{t-1} < BS  (BZ 论文 step 5)
    // t = floor(bitlen(AS) / (64n)) + 1
    const int64_t abits = (int64_t)64 * (nas - 1) + (64 - (int)_lzcnt_u64(AS[nas - 1]));
    int t = (int)(abits / ((int64_t)64 * n)) + 1;
    if (t < 2) t = 2;

    // Z = AS 的最高两块 (2n limbs)，不足处补 0
    u64* Z = wp; wp += 2 * n + 8;
    u64* Qi = wp; wp += n + 8;
    std::memset(Q, 0, (size_t)(na - nb + 1) * 8);

    auto load_block = [&](int idx, u64* dst) {          // AS 的第 idx 块 (n limbs)
        int off = idx * n;
        int cnt = nas - off;
        if (cnt > n) cnt = n;
        if (cnt <= 0) { std::memset(dst, 0, (size_t)n * 8); return; }
        std::memcpy(dst, AS + off, (size_t)cnt * 8);
        if (cnt < n) std::memset(dst + cnt, 0, (size_t)(n - cnt) * 8);
    };

    if (t >= 3 || n >= BARRETT_NMIN) {
        // ---- Barrett: 预计算 V = floor((2^(128n)-1)/BS) = 2^(64n) + VB, 每块只需 2 次 n*n 乘法 ----
        u64* q2 = VB;
        invertappr(BS, n, q2);                              // Newton 倒数, ~2.5*M(n)
        u64* P = wp; wp += 2 * n + 8;
        fm_prep(FF1, FMG1, q2, n, n);                       // 固定乘数 q2 的正变换
        fm_prep(FF2, FMG2, BS, n, n);                       // 固定乘数 BS 的正变换
        load_block(t - 1, Z + n);
        load_block(t - 2, Z);
        for (int i = t - 2; i >= 0; --i) {
            u64* Z1 = Z + n;                                // 高半 (= 上轮余数, < BS)
            if (FF1.ok) fm_mul(FF1, FMG1, Z1, n, P);        // P = Z1 * q2
            else mulg(Z1, n, q2, n, P);
            unsigned char c = 0;                            // qhat = Z1 + hi_n(Z1*q2)
            for (int k = 0; k < n; ++k)
                c = _addcarry_u64(c, Z1[k], P[n + k], (unsigned long long*)&Qi[k]);
            if (FF2.ok) fm_mul(FF2, FMG2, Qi, n, P);        // P = qhat * BS
            else mulg(Qi, n, BS, n, P);
            unsigned char br = 0;                           // Z -= P   (2n limbs)
            for (int k = 0; k < 2 * n; ++k)
                br = _subborrow_u64(br, Z[k], P[k], (unsigned long long*)&Z[k]);
            for (;;) {                                      // 修正: 至多 ~6 轮
                bool ge = false;
                for (int k = 2 * n - 1; k >= n; --k) if (Z[k]) { ge = true; break; }
                if (!ge && cmpn(Z, BS, n) >= 0) ge = true;
                if (!ge) break;
                unsigned char b2 = 0;
                for (int k = 0; k < n; ++k)
                    b2 = _subborrow_u64(b2, Z[k], BS[k], (unsigned long long*)&Z[k]);
                for (int k = n; b2; ++k) { u64 cur = Z[k]; Z[k] = cur - b2; b2 = (cur < b2); }
                for (int k = 0; k < n; ++k) if (++Qi[k]) break;
            }
            {
                int off = i * n;
                int lim = na - nb + 1 - off;
                if (lim > n) lim = n;
                if (lim > 0) std::memcpy(Q + off, Qi, (size_t)lim * 8);
            }
            std::memcpy(Z + n, Z, (size_t)n * 8);           // 余数搬到高半, 供下一轮
            if (i > 0) load_block(i - 1, Z);
        }
    } else {
        load_block(t - 1, Z + n);
        load_block(t - 2, Z);
        for (int i = t - 2; i > 0; --i) {
            div_2n_1n(Z, BS, n, Qi, Z + n);                 // 余数直接落回 Z 的高半区
            // Q += Qi << (64*n*i)
            {
                int off = i * n;
                int lim = na - nb + 1 - off;
                if (lim > n) lim = n;
                if (lim > 0) std::memcpy(Q + off, Qi, (size_t)lim * 8);
            }
            load_block(i - 1, Z);
        }
        div_2n_1n(Z, BS, n, Qi, Z + n);
        {
            int lim = na - nb + 1;
            if (lim > n) lim = n;
            std::memcpy(Q, Qi, (size_t)lim * 8);
        }
    }
    // R = (Z 高半区) >> sigma
    const u64* Rs = Z + n;
    {
        u64* dst = R;
        if (sb) {
            for (int i = 0; i < nb; ++i) {
                int k2 = i + sw;
                u64 lo = (k2 < n) ? (Rs[k2] >> sb) : 0;
                u64 hi = (k2 + 1 < n) ? (Rs[k2 + 1] << (64 - sb)) : 0;
                dst[i] = lo | hi;
            }
        } else {
            for (int i = 0; i < nb; ++i) dst[i] = (i + sw < n) ? Rs[i + sw] : 0;
        }
    }
    wp = Z;
}

static inline int mag_cmp(const u64* a, int na, const u64* b, int nb) {
    if (na != nb) return na > nb ? 1 : -1;
    return cmpn(a, b, na);
}

// ============================ divrem_1 ============================
static u64 divrem_1(const u64* a, int na, u64 d, u64* q) {
    if (d == 1) { std::memcpy(q, a, (size_t)na * 8); return 0; }
    u64 rem = 0;
    for (int i = na - 1; i >= 0; --i) {
        u128 cur = ((u128)rem << 64) | a[i];
        q[i] = (u64)(cur / d);
        rem = (u64)(cur % d);
    }
    return rem;
}

int main() {
    int len = 0;
    for (;;) {
        long r = read(0, inbuf + len, INCAP - len);
        if (r <= 0) break;
        len += (int)r;
    }
    std::memset(inbuf + len, 0, 96);
    inbuf[len] = '\n';

    const char* p = inbuf;
    while (*p < '0') ++p;
    u32 T = 0;
    while (*p > ' ') T = T * 10 + (u32)(*p++ - '0');

    char* out = outbuf;

    for (u32 t = 0; t < T; ++t) {
        while (*p <= ' ') ++p;
        const char* a0 = p;
        int la = tok_len(p); p += la;
        while (*p <= ' ') ++p;
        const char* b0 = p;
        int lb = tok_len(p); p += lb;

        if (la <= 16 && lb <= 16) {
            u64 av = hexpart(a0 + la, la), bv = hexpart(b0 + lb, lb);
            u64 qq = av / bv, rr = av - qq * bv;
            out = put_u64(out, qq);
            *out++ = ' ';
            out = put_u64(out, rr);
            *out++ = '\n';
            continue;
        }
        if (la <= 32 && lb <= 16) {
            u128 av = ((u128)hexpart(a0 + la - 16, la - 16) << 64) | hexfull(a0 + la);
            u64 bv = hexpart(b0 + lb, lb);
            u128 qq = av / bv;
            u64 rr = (u64)(av - qq * bv);
            u64 qh = (u64)(qq >> 64), ql = (u64)qq;
            if (qh) { out = put_u64(out, qh); hex16_store(ql, out); out += 16; }
            else out = put_u64(out, ql);
            *out++ = ' ';
            out = put_u64(out, rr);
            *out++ = '\n';
            continue;
        }

        int na = parse_limbs(a0, la, A);
        int nb = parse_limbs(b0, lb, B);
        while (na > 1 && A[na - 1] == 0) --na;
        while (nb > 1 && B[nb - 1] == 0) --nb;

        if (na == 1 && A[0] == 0) { *out++ = '0'; *out++ = ' '; *out++ = '0'; *out++ = '\n'; continue; }
        if (mag_cmp(A, na, B, nb) < 0) {
            *out++ = '0'; *out++ = ' ';
            out = put_big(out, A, na);
            *out++ = '\n';
            continue;
        }
        if (nb == 1) {
            u64 rr = divrem_1(A, na, B[0], Qout);
            out = put_big(out, Qout, na);
            *out++ = ' ';
            out = put_u64(out, rr);
            *out++ = '\n';
            continue;
        }
        // 商 limb 数 = na-nb+1。极短商时 Knuth D 只跑几轮 O(nb)，远快于 BZ 的 M(nb)logn
        if (nb < BZ_MIN || na - nb + 1 <= KD_QMAX) {
            knuthD(A, na, B, nb, Qout, Rout);
        } else {
            wp = WORK;
            bz_divide(A, na, B, nb, Qout, Rout);
        }
        out = put_big(out, Qout, na - nb + 1);
        *out++ = ' ';
        out = put_big(out, Rout, nb);
        *out++ = '\n';
    }

    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    return 0;
}
