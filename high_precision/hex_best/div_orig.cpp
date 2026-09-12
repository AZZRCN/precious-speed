// 喵喵喵~ https://space.bilibili.com/620657947
/*
Submission #392237
ID	Date	Problem	Lang	User	Status	Time	Memory
392237	2026/8/12 18:17:06	

Division of Hex Big Integers
	C++23	(Anonymous)	AC	47 ms	31.52 Mib
Name	Status	Time	Memory
example_00	AC	1 ms	4.78 Mib
small_00	AC	9 ms	12.26 Mib
medium_00	AC	7 ms	9.79 Mib
medium_01	AC	2 ms	6.66 Mib
medium_02	AC	2 ms	7.54 Mib
large_00	AC	2 ms	7.04 Mib
large_01	AC	3 ms	8.54 Mib
max_00	AC	3 ms	8.29 Mib
max_01	AC	2 ms	6.29 Mib
max_02	AC	3 ms	9.51 Mib
a_max_b_random_00	AC	21 ms	17.51 Mib
a_max_b_random_01	AC	33 ms	28.79 Mib
a_max_b_random_02	AC	33 ms	31.52 Mib
power_00	AC	3 ms	6.79 Mib
r_nearly_zero_00	AC	10 ms	8.28 Mib
r_nearly_zero_01	AC	2 ms	7.26 Mib
r_nearly_zero_02	AC	2 ms	7.03 Mib
length_ratio_integer_00	AC	47 ms	31.30 Mib
length_ratio_integer_01	AC	39 ms	23.25 Mib
length_ratio_integer_02	AC	45 ms	23.76 Mib
length_ratio_integer_03	AC	42 ms	22.51 Mib
length_ratio_integer_04	AC	39 ms	24.04 Mib
length_ratio_integer_05	AC	27 ms	24.05 Mib
burnikel_ziegler_bound_00	AC	11 ms	10.76 Mib
burnikel_ziegler_bound_01	AC	18 ms	20.76 Mib
burnikel_ziegler_bound_02	AC	6 ms	9.29 Mib
burnikel_ziegler_bound_03	AC	16 ms	20.96 Mib
*/
// AZZRCN
// https://github.com/AZZRCN
// HEX division v11 (v10 + MADV_HUGEPAGE 巨页)  (A>=0, B>0, floor 除法, 输出 "q r")
//   v11 = v10(vectorized split_b2, LEAF=10) + 全大缓冲 alignas(2MiB) + hugify():
//   消除 ~9700 次 minor fault (参考 391969 的 17ms 路径)。idle VM 大用例 -5~-6ms(-10%)。
//   正确性: VM 27/27 hexcheck 闸门全过。LC 未提交 (铁律#2)。
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
#include <vector>
#include <immintrin.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/mman.h>
#endif

static constexpr size_t HP = 1u << 21;   // 2 MiB 透明大页
// THP=madvise 时内核不主动给 2 MiB 页, 必须显式申请; LC THP=always 无害, never 退化无害。
static inline void hugify(void* p, size_t n) {
#ifdef MADV_HUGEPAGE
    madvise(p, n, MADV_HUGEPAGE);
#endif
}

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

alignas(HP) static char inbuf_[PAD + INCAP + PAD];
alignas(HP) static char outbuf[OUTCAP + 256];
static char* const inbuf = inbuf_ + PAD;
alignas(HP) static u64 A[MAXC + 8], B[MAXC + 8];
alignas(HP) static u64 Qout[MAXC + 8], Rout[MAXC + 8];
alignas(HP) static u64 AN[2 * MAXC + 16], BN[MAXC + 8];  // Knuth D 归一化缓冲
alignas(HP) static u64 AS[MAXC + 8192], BS[MAXC + 8192]; // BZ 顶层 shift 缓冲 (块长 n 可略大于 nb)
alignas(HP) static u64 VB[MAXC + 8192];                  // Barrett 倒数 V 的低 n limbs (V = 2^64n + VB)
alignas(HP) static u64 WORK[4000000];                    // Newton/BZ 工作栈 (~32MB), 峰值约 16*n
static u64* wp = WORK;

static constexpr u32 LMMAX = 1u << 20;
alignas(HP) static double FB[LMMAX], GB[LMMAX];

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
#ifndef FFT_LEAF_LOG
// 选 13: v11(hugify) 基础上扩扫 9..13, idle best-of-5 大用例全优于 10/11/12
// (length_ratio_integer_00 48 vs 51/53/53; a_max_b_random 34~36 最优)。与 MUL 同取 13。
#define FFT_LEAF_LOG 11
#endif
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
        for (u32 bs = 2, be = 3; bs != n; bs <<= 1, be <<= 1) {
            for (u32 f = bs, b = f + bs - 1; f != be; ++f, --b) {
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

// ============================ bit split / merge ============================
// AVX2 gather 位提取: 一条 vpgatherdd 抓 8 个跨界 32-bit 窗口, 变量移位 + 掩码 + 转 double。
// 要求 k <= 25 (窗口 k+7 位必须落在 32 位内), 实际 k <= 19。
static size_t split_b2(const u64* src, double* g, size_t n, int k, size_t zlim) {
    const size_t total = ((n << 6) + (size_t)k - 1) / (size_t)k;
    const unsigned char* base = (const unsigned char*)src;
    const u32 m = (1u << k) - 1;
    size_t j = 0;
    // 向量段: 保证 gather 读取的 4 字节完全落在 [0, 8n)
    size_t vend = ((n << 6) >= 32) ? ((n << 6) - 32) / (size_t)k : 0;
    if (vend > total) vend = total;
    vend &= ~(size_t)7;
    if (vend) {
        const __m256i off0 = _mm256_setr_epi32(0, k, 2 * k, 3 * k, 4 * k, 5 * k, 6 * k, 7 * k);
        const __m256i step = _mm256_set1_epi32(8 * k);
        const __m256i mv = _mm256_set1_epi32((int)m);
        const __m256i m7 = _mm256_set1_epi32(7);
        __m256i bit = off0;
        for (; j < vend; j += 8) {
            __m256i bo = _mm256_srli_epi32(bit, 3);
            __m256i sh = _mm256_and_si256(bit, m7);
            __m256i w = _mm256_i32gather_epi32((const int*)base, bo, 1);
            w = _mm256_and_si256(_mm256_srlv_epi32(w, sh), mv);
            _mm256_storeu_pd(g + j, _mm256_cvtepi32_pd(_mm256_castsi256_si128(w)));
            _mm256_storeu_pd(g + j + 4, _mm256_cvtepi32_pd(_mm256_extracti128_si256(w, 1)));
            bit = _mm256_add_epi32(bit, step);
        }
    }
    for (; j < total; ++j) {  // 安全标量尾 (末尾高位补 0)
        const size_t b = j * (size_t)k;
        const size_t li = b >> 6;
        const int sh = (int)(b & 63);
        u64 v = (li < n) ? (src[li] >> sh) : 0;
        if (sh && li + 1 < n) v |= src[li + 1] << (64 - sh);
        g[j] = (double)(u32)(v & m);
    }
    // 只清尾部: 前 total 项已被写满, 全量 memset 是冗余双写
    if (zlim > total) std::memset(g + total, 0, (zlim - total) * 8);
    return total;
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
// FFT 实系数槽位数: 严格 ceil + 严格 next_pow2。
// 旧实现 (u*64/k + 1) 与 (2u << (31-clz(c))) 在 c 恰为 2 的幂时会白白多要一倍长度。
// 返回 >= c 的最小可用 FFT 长度(doubles): 2^k / 3*2^(k-2) / 5*2^(k-3).
// 混合档位恒 <= next_pow2(c), 故 2^(2k)*lm <= 2^48 精度预算天然不破.
static inline u32 fft_ceil_tiers(u32 c) {
    u32 p = (c & (c - 1)) ? (2u << (31 - __builtin_clz(c))) : c;  // next_pow2(c)
    u32 best = p;
#ifdef V16_NOMIX
    return best;   // 归因对照: 关掉混合档, 只留 2 幂 (代码仍在, 隔离 "代码膨胀/布局" 与 "混合路径本身")
#endif
    // 护栏: 块内复数数 m 必须 >= 16 (pointwise_blk 的 F[0]/F[1] 特例 + 层循环需要),
    //       radix-3 时 m = p/8, radix-5 时 m = p/16 -> p >= 1024 一并覆盖 (m >= 64).
    if (c != p && p >= 1024) {
        u32 h = (p >> 2) * 3;          // 3*2^(k-2)
        if (h >= c && h < best) best = h;
        u32 h5 = (p >> 3) * 5;         // 5*2^(k-3)
        if (h5 >= c && h5 < best) best = h5;
    }
    return best;
}
static inline u32 fft_len_for(size_t u, int k) {
    const u32 c = (u32)((u * 64 + (size_t)k - 1) / (size_t)k);
    return fft_ceil_tiers(c);
}
static inline int pick_k(size_t u) {
    static const size_t gk[11] = {19ull << 4,  18ull << 6,  17ull << 8, 16ull << 10,
                                  15ull << 12, 14ull << 14, 13ull << 16, 12ull << 18,
                                  11ull << 20, 10ull << 22, ~size_t(0)};
    int i = 0;
    while (u > gk[i]) ++i;
    return 19 - i;
}

// ============================ multiply ============================
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
    const u32 lm = fft_len_for(u, k);
    const bool same = (a == b) && (na == nb);
    const u32 ts = lm >> 1;                 // 总复数点数
    // 路径判定必须在 split 之前: zero-hi 只适用于纯 2 幂路径 (混合路径顶层未做半区跳过),
    // 若在这里错误地把 zlim 提成 lm, 就等于废掉 v13 的 zero-hi (省一半清零 + difRecZeroHi 省一半读流量).
    const int path = (lm % 3 == 0) ? 3 : ((lm % 5 == 0) ? 5 : 2);
    // zero-hi: 单个操作数系数数 <= lm/2 时, 实数缓冲上半整块为零,
    // 顶层 radix-4 可只读下半 (省 1/2 读流量 + split 省 1/2 清零)。
    const bool deep = (path == 2) && ts > (1u << FFT_LEAF_LOG);
    const size_t half = lm >> 1;
    const size_t ta = split_b2(a, FB, na, k, deep ? half : lm);
    const bool hiA = deep && ta <= half;
    // deep 且未命中 zero-hi 时 split 只清到 half(<ta) 等于没清, 需补清 [ta, lm)
    if (deep && !hiA && ta < lm) std::memset(FB + ta, 0, (lm - ta) * 8);
    size_t tb = ta; bool hiB = hiA;
    if (!same) {
        tb = split_b2(b, GB, nb, k, deep ? half : lm);
        hiB = deep && tb <= half;
        if (deep && !hiB && tb < lm) std::memset(GB + tb, 0, (lm - tb) * 8);
    }
    // ---- 混合 radix 路径 (全复数; 点乘复用现成 pointwise/pointwiseSq 的共轭对称归一化) ----
    // 调度与 fft_ceil_tiers 严格对应: 3 整除 <=> 3*2^(k-2) (m=ts/3 为 2 幂);
    // 5 整除 <=> 5*2^(k-3) (m=ts/5 为 2 幂); 否则纯 2 幂. 三者互斥且穷尽.
    if (path == 3) {
        const u32 m = ts / 3;               // 每块复数数 (2 的幂)
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)FB, m);
        fft::difRec((fft::cpx*)FB,                 m, 0);
        fft::difRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::difRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        if (same) {
            fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)FB, m, 3);
        } else {
            fft::dif3StageR((fft::cpx*)GB, m);
            fft::difRec((fft::cpx*)GB,                 m, 0);
            fft::difRec((fft::cpx*)(GB + 2*(size_t)m), m, 0);
            fft::difRec((fft::cpx*)(GB + 4*(size_t)m), m, 0);
            fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)GB, m, 3);
        }
        fft::ditRec((fft::cpx*)FB,                 m, 0);
        fft::ditRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::ditRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        fft::idit3StageR((fft::cpx*)FB, m);
    } else if (path == 5) {
        const u32 m = ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)FB, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        if (same) {
            fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)FB, m, 5);
        } else {
            fft::dif5StageR((fft::cpx*)GB, m);
            for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(GB + 2*(size_t)m*i), m, 0);
            fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)GB, m, 5);
        }
        for (int i = 0; i < 5; ++i) fft::ditRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::idit5StageR((fft::cpx*)FB, m);
    } else {
        // ---- 纯 2 幂路径 (原 v13, 含 zero-hi 优化) ----
        fft::resize(ts);
        if (hiA) fft::difRecZeroHi((fft::cpx*)FB, ts); else fft::difRec((fft::cpx*)FB, ts, 0);
        if (same) {
            fft::pointwiseSq((fft::cpx*)FB, ts);
        } else {
            if (hiB) fft::difRecZeroHi((fft::cpx*)GB, ts); else fft::difRec((fft::cpx*)GB, ts, 0);
            fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
        }
        fft::ditRec((fft::cpx*)FB, ts, 0);
    }
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
alignas(HP) static double FMG1[FMCAP], FMG2[FMCAP];
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
    split_b2(b, G, nb, F.k, F.lm);
    fft::resize(F.ts);
    fft::difRec((fft::cpx*)G, F.ts, 0);
    F.ok = true;
}
static void fm_mul(const FixedFFT& F, double* G, const u64* a, int na, u64* c) {
    std::memset(FB, 0, (size_t)F.lm * 8);
    split_b2(a, FB, na, F.k, F.lm);
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

// ============================ Newton 倒数主除法 ============================
// a(na) / d(nb) -> q(na-nb+1), r(nb)。要求 a>=d>0 且 na>=nb。
// 思路: 归一化使 d 顶位=1; 用 invertappr 求 v≈β^{2n}/d - β^n;
//   q_est = floor(a·(β^n+v)/β^{2n}) = floor(a/β^n) + floor(a·v/β^{2n});
//   校正: R = a - q_est·d (至多 +2 轮) 得精确 q,r; r>>sigma 还原。
static void newton_divide(const u64* a, int na, const u64* d, int nb, u64* q, u64* r) {
    const int n = nb;
    const int sigma = (int)_lzcnt_u64(d[n - 1]);   // 0..63
    u64* save = wp;
    u64* dn = wp; wp += n + 1;
    u64* an = wp; wp += na + 1;
    u64* v  = wp; wp += n + 1;
    // 归一化 (左移 sigma 使 d[n-1] 顶位=1)
    if (sigma) {
        unsigned char c = 0;
        for (int i = 0; i < n; ++i) { u64 cur = d[i]; dn[i] = (cur << sigma) | c; c = cur >> (64 - sigma); }
        c = 0;
        for (int i = 0; i < na; ++i) { u64 cur = a[i]; an[i] = (cur << sigma) | c; c = cur >> (64 - sigma); }
        an[na] = c;
    } else {
        std::memcpy(dn, d, (size_t)n * 8);
        std::memcpy(an, a, (size_t)na * 8);
        an[na] = 0;
    }
    const int na_an = na + 1;
    invertappr(dn, n, v);                           // Newton 倒数 (内部用 wp, 退出还原)
    u64* L = wp; wp += na_an + n + 1;
    mulg(an, na_an, v, n, L);                       // L = an * v
    const int qn = na - n + 1;
    u64* qe = wp; wp += qn + 1;
    std::memset(qe, 0, (size_t)(qn + 1) * 8);
    {                                               // q_est = floor(an/β^n) + floor(an*v/β^{2n})
        unsigned char c = 0;
        for (int i = 0; i < qn; ++i) {
            u64 x = (n + i < na_an) ? an[n + i] : 0;
            u64 y = (2 * n + i < na_an + n) ? L[2 * n + i] : 0;
            u64 s; c = _addcarry_u64(c, x, y, (unsigned long long*)&s);
            qe[i] = s;
        }
        qe[qn] = c;
    }
    // 模进位: (an*β^n mod β^{2n}) + (L mod β^{2n}) >= β^{2n} 时 q_est 需 +1
    {
        unsigned char c = 0;
        for (int i = 0; i < 2 * n; ++i) {
            u64 x = (i >= n) ? an[i - n] : 0;
            u64 s; c = _addcarry_u64(c, x, L[i], (unsigned long long*)&s);
        }
        if (c) {
            unsigned char cc = 1;
            for (int i = 0; i < qn + 1 && cc; ++i) { u64 cur = qe[i]; qe[i] = cur + cc; cc = (cur + cc < cur); }
        }
    }
    u64* QD = wp; wp += na_an + 1;
    std::memset(QD, 0, (size_t)(na_an + 1) * 8);
    mulg(qe, qn, dn, n, QD);                        // QD = qe * dn
    u64* Rt = wp; wp += na_an + 1;
    {                                               // Rt = an - QD
        unsigned char b = 0;
        for (int i = 0; i < na_an; ++i) {
            u64 sd = QD[i];
            u64 s; b = _subborrow_u64(b, an[i], sd, (unsigned long long*)&s);
            Rt[i] = s;
        }
        if (b) {                                    // 理论不发生: qe 偏大 1, 回退
            unsigned char cc = 1;
            for (int i = 0; i < qn && cc; ++i) { u64 cur = qe[i]; qe[i] = cur - cc; cc = (cur < cc); }
            unsigned char ad = 0;
            for (int i = 0; i < na_an; ++i) { u64 sd = (i < n) ? dn[i] : 0; u64 s; ad = _addcarry_u64(ad, Rt[i], sd, (unsigned long long*)&s); Rt[i] = s; }
        }
    }
    // 校正: invertappr 低估 => q_est <= q。余数 Rt >= dn 时说明商偏小, 必须 qe++, Rt -= dn。
    for (int iter = 0; iter < 32; ++iter) {
        bool ge = false;
        for (int i = na_an - 1; i >= n; --i) if (Rt[i]) { ge = true; break; }
        if (!ge) {
            int cmp = 0;
            for (int i = n - 1; i >= 0; --i) { if (Rt[i] != dn[i]) { cmp = (Rt[i] > dn[i]) ? 1 : -1; break; } }
            ge = (cmp >= 0);
        }
        if (!ge) break;
        unsigned char cc = 1;                          // qe++
        for (int i = 0; i < qn; ++i) { u64 s; cc = _addcarry_u64(cc, qe[i], 0, (unsigned long long*)&s); qe[i] = s; if (!cc) break; }
        unsigned char sb = 0;                          // Rt -= dn
        for (int i = 0; i < na_an; ++i) { u64 sd = (i < n) ? dn[i] : 0; u64 s; sb = _subborrow_u64(sb, Rt[i], sd, (unsigned long long*)&s); Rt[i] = s; }
    }
    std::memcpy(q, qe, (size_t)qn * 8);
    if (sigma) {
        for (int i = 0; i < n; ++i) {
            u64 lo = Rt[i] >> sigma;
            u64 hi = (i + 1 < na_an) ? (Rt[i + 1] << (64 - sigma)) : 0;
            r[i] = lo | hi;
        }
    } else {
        std::memcpy(r, Rt, (size_t)n * 8);
    }
    wp = save;
}

int main() {
    // 巨页: 大缓冲全部对齐 2MiB 并显式申请, 消除 minor fault (参考 391969 的 17ms 路径)
    hugify(inbuf_, sizeof inbuf_);
    hugify(outbuf, sizeof outbuf);
    hugify(A, sizeof A); hugify(B, sizeof B);
    hugify(Qout, sizeof Qout); hugify(Rout, sizeof Rout);
    hugify(AN, sizeof AN); hugify(BN, sizeof BN);
    hugify(AS, sizeof AS); hugify(BS, sizeof BS); hugify(VB, sizeof VB);
    hugify(WORK, sizeof WORK);
    hugify(FB, sizeof FB); hugify(GB, sizeof GB);
    hugify(FMG1, sizeof FMG1); hugify(FMG2, sizeof FMG2);
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
        } else if (na <= 2 * nb) {
            wp = WORK;
            newton_divide(A, na, B, nb, Qout, Rout);
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
