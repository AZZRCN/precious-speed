/*
Submission #392043
ID	Date	Problem	Lang	User	Status	Time	Memory
392043	2026/8/12 01:01:41	
Division of Hex Big Integers
	C++23	(Anonymous)	AC	52 ms	29.01 Mib
	Name	Status	Time	Memory
	example_00	AC	1 ms	0.79 Mib
	small_00	AC	13 ms	10.54 Mib
	medium_00	AC	19 ms	8.55 Mib
	medium_01	AC	4 ms	6.55 Mib
	medium_02	AC	4 ms	6.28 Mib
	large_00	AC	4 ms	5.79 Mib
	large_01	AC	4 ms	5.01 Mib
	max_00	AC	5 ms	6.64 Mib
	max_01	AC	3 ms	6.04 Mib
	max_02	AC	4 ms	7.82 Mib
	a_max_b_random_00	AC	23 ms	11.76 Mib
	a_max_b_random_01	AC	41 ms	25.53 Mib
	a_max_b_random_02	AC	40 ms	29.01 Mib
	power_00	AC	4 ms	4.79 Mib
	r_nearly_zero_00	AC	23 ms	7.60 Mib
	r_nearly_zero_01	AC	3 ms	4.32 Mib
	r_nearly_zero_02	AC	3 ms	4.01 Mib
	length_ratio_integer_00	AC	52 ms	26.30 Mib
	length_ratio_integer_01	AC	42 ms	20.27 Mib
	length_ratio_integer_02	AC	48 ms	18.52 Mib
	length_ratio_integer_03	AC	44 ms	15.77 Mib
	length_ratio_integer_04	AC	40 ms	14.04 Mib
	length_ratio_integer_05	AC	27 ms	12.75 Mib
	burnikel_ziegler_bound_00	AC	12 ms	7.79 Mib
	burnikel_ziegler_bound_01	AC	19 ms	8.04 Mib
	burnikel_ziegler_bound_02	AC	9 ms	6.02 Mib
	burnikel_ziegler_bound_03	AC	17 ms	7.97 Mib
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
alignas(HP) static u64 WORK[3200000];                    // BZ/Barrett 工作栈 (~25.6MB), 峰值约 16*n
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
#define FFT_LEAF_LOG 13
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
}  // namespace fft