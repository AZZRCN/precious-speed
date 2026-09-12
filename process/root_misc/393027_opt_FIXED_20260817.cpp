
// 喵喵喵~ https://space.bilibili.com/620657947
/*Submission #393027
ID	Date	Problem	Lang	User	Status	Time	Memory
393027	2026/8/14 09:14:03	

Division of Hex Big Integers
	C++23	(Anonymous)	AC	39 ms	29.21 Mib
	Name	Status	Time	Memory
	example_00	AC	1 ms	2.79 Mib
	small_00	AC	11 ms	11.52 Mib
	medium_00	AC	7 ms	8.80 Mib
	medium_01	AC	3 ms	6.79 Mib
	medium_02	AC	2 ms	7.54 Mib
	large_00	AC	2 ms	7.80 Mib
	large_01	AC	2 ms	7.01 Mib
	max_00	AC	4 ms	10.51 Mib
	max_01	AC	3 ms	7.80 Mib
	max_02	AC	3 ms	9.80 Mib
	a_max_b_random_00	AC	21 ms	17.70 Mib
	a_max_b_random_01	AC	32 ms	29.21 Mib
	a_max_b_random_02	AC	34 ms	29.00 Mib
	power_00	AC	3 ms	6.75 Mib
	r_nearly_zero_00	AC	9 ms	7.29 Mib
	r_nearly_zero_01	AC	2 ms	6.79 Mib
	r_nearly_zero_02	AC	2 ms	7.29 Mib
	length_ratio_integer_00	AC	39 ms	23.04 Mib
	length_ratio_integer_01	AC	35 ms	21.77 Mib
	length_ratio_integer_02	AC	36 ms	23.55 Mib
	length_ratio_integer_03	AC	33 ms	24.01 Mib
	length_ratio_integer_04	AC	31 ms	22.54 Mib
	length_ratio_integer_05	AC	35 ms	23.54 Mib
	burnikel_ziegler_bound_00	AC	10 ms	9.76 Mib
	burnikel_ziegler_bound_01	AC	23 ms	20.51 Mib
	burnikel_ziegler_bound_02	AC	6 ms	9.01 Mib
	burnikel_ziegler_bound_03	AC	14 ms	19.21 Mib
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
#include <cstdlib>
#ifdef INV_VERIFY
#include <cstdio>
#endif
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
// v27-INCAPfix: 输入缓冲容量。原 9<<20 仅容单个数 <= 562500 limb (9M 字符); 但输入格式每 query 含两个数,
// 且 LC 官方 length_ratio_integer 为 T=2 大 query, 总输入可达 2*9M+ ~ 19M 字符 -> 只读入 9M 即截断,
// query2 第二个数越界读穿 inbuf -> 多 query Heisenbug 崩溃 (t2d: 260000 limb × 2 × T=2 = 16.6M > 9M)。
// 放大到 20<<20: 容纳两个 MAXC(600000 limb) 大数 (19.2M) + T/分隔符余量。
static constexpr int INCAP = 20 << 20;
static constexpr int OUTCAP = 10 << 20;
// v27-MAXCfix: 原 100010 假设输入 <= 100010 limb, 但 INCAP=9M 字符允许单个数达 ~562500 limb;
// 官方 length_ratio_integer m=2 (B~266666 limb, T=2) 远超 -> Rout/Qout/A/B 等缓冲越界 (query2 Heisenbug 崩溃)。
// 放大到 600000: A/B/Qout/Rout 容 600k limb, AN=2*MAXC 容 A 达 1.2M, 覆盖 INCAP 上限并留余量。
static constexpr int MAXC = 600000;
#ifndef BZ_CUTOFF
#define BZ_CUTOFF 64                         // BZ 叶子规模 (limbs)
#endif
#ifndef BZ_MIN
#define BZ_MIN (BZ_CUTOFF * 2 + 32)          // 低于此规模直接 Knuth D
#endif
#ifndef KD_QMAX
#define KD_QMAX 128                          // 商 limb 数 <= 此值时 Knuth D 完胜 BZ (285H callgrind: 128 比 64 省 ~1.5%)
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
// v27-WORKfix: BZ 顶层 shift 缓冲。AS/BS 索引可达 sw+na (sw=sigma>>6 位移, na<=MAXC),
// 块长 n 可略大于 nb, 故 sw+na 最坏 ~2*MAXC -> 放大到 2*MAXC+8192 防越界。
alignas(HP) static u64 AS[2 * MAXC + 8192], BS[2 * MAXC + 8192];
alignas(HP) static u64 VB[MAXC + 8192];                  // Barrett 倒数 V 的低 n limbs (V = 2^64n + VB); 仅写 n limbs, n<=MAXC
// v27-WORKfix: Newton/BZ 工作栈。峰值约 16*n; 官方/INCAP 上限 nb<=MAXC=600000 -> 16*MAXC=9.6M。
// 原 4000000 在 n~262144 (t2d: 260000 limb 巨例) 时 16*n=4.2M>4M -> 溢出写穿邻接全局 -> 多 query Heisenbug 崩溃。
// 放大到 12M (覆盖 MAXC 并留 ~25% 余量)。
alignas(HP) static u64 WORK[12000000];                   // Newton/BZ 工作栈 (~96MB), 峰值约 16*n
static u64* wp = WORK;

// v27-FBfix: FB/GB 须容纳 mul_fft 的 FFT 长度 lm (difRec 写 ts=lm>>1 复数 = lm double)。
// 原 1<<20 仅容 lm<=1M；大输入 (u>~262144) fft_len_for 产出 lm 达 2M~4M+ -> FB 溢出 (fortify 在 mul_fft L1100 抓到)。
// 放大到 1<<23 (8M double) 覆盖官方 length_ratio_integer m=2 (B~266666 limb, lm~4.2M) 并留 2x 余量。
static constexpr u32 LMMAX = 1u << 23;
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
// v18-P2a: noinline 是实测优化, 非风格选择。merge_b2 有 6 处调用点 (mul_fft path2/3/5 +
//   fm_mul + cyc_mul_fixed + cyc 线性回退), 内联后每处一份代码, 寄存器压力上升 + 循环体劣化。
//   VM66 callgrind: 内联 416,990,433 -> noinline 411,164,149 (-5,826,284, -1.397%), 26-case 字节 SAME。
__attribute__((noinline)) static void merge_b2(u64* f, const double* g, size_t n, int k) {
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
    // v27-FBfix: 容量守卫。lm > LMMAX 时回退 schoolbook (正确安全网; 官方尺寸 lm<=~4.2M < 8M 永不触发)。
    // 若未来出现更大输入, 此处应改为 Karatsuba-3 递归 (FFT 叶) 以免 TLE; 当前仅作防越界底线。
    if (lm > LMMAX) { mul_bf(a, na, b, nb, c); return; }
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
// v17: lm 由「纯 2 幂 next_pow2(coeffs)」改为 fft_ceil_tiers (2^j / 3*2^(j-2) / 5*2^(j-3))。
//   旧实现在 coeffs 略过 2 幂时白付 2x 长度 —— 例如 n=16896: coeffs=144226 -> 旧 262144, 新 163840,
//   此时 fm_mul(2 次 131072 点变换 = 262144) 竟比普通 mulg(3 次 81920 = 245760) 还慢, 属净亏。
//   混合档恒 <= next_pow2, 精度预算 2^(2k)*lm <= 2^48 天然不破。
struct FixedFFT { size_t u; u32 lm, ts; int k, path; bool ok; };
alignas(HP) static double FMG1[FMCAP], FMG2[FMCAP], FMG3[FMCAP];
// v23: invertappr 内 xh 的固定环形正变换缓冲。与 FMG1..3 (bz_divide Barrett 循环) 无重叠 ——
//   invertappr 在 Barrett 循环之前完成; 且递归各层的两次乘法都发生在深层返回之后, 故单缓冲够用。
alignas(HP) static double FMG4[FMCAP];
static FixedFFT FF1, FF2;
static bool g_fmt = true;                  // FMT=0 -> 退回旧的纯 2 幂 (A/B 归因用)
static bool g_bzall = false;                // BZALL=1 -> na<=2nb 也路由 BZ (实验: 看 BZ 是否更便宜)

// v19-B: 尺寸规划与正变换拆开。bz_divide 里 use_cyc 判据需要 FF2.lm, 但若判定走环形路径,
//   FF2 的正变换就完全用不上 —— 旧码无条件 fm_prep(FF2) 白付一次全长正变换 (lm=327680 档约 13.6M Ir/query)。
static bool fm_plan(FixedFFT& F, int nb, int na) {
    F.ok = false;
    if (nb <= MULBF_MAX) return false;
    F.u = (size_t)na + nb;
    F.k = pick_k(F.u);
    if (g_fmt) {
        F.lm = fft_len_for(F.u, F.k);
    } else {
        const u32 coeffs = (u32)(F.u * 64 / F.k) + 1;
        F.lm = 2u << (31 - __builtin_clz(coeffs));
    }
    if (F.lm > FMCAP) return false;
    F.ts = F.lm >> 1;
    F.path = (F.lm % 3 == 0) ? 3 : ((F.lm % 5 == 0) ? 5 : 2);
    return true;
}
static void fm_prep(FixedFFT& F, double* G, const u64* b, int nb, int na) {
    if (!fm_plan(F, nb, na)) return;
    // v18-P2b: 前置全量 memset 已删 —— split_b2(zlim=lm) 写满 [0,total) 并自清 [total,lm),
    //   [0,lm) 全覆盖, 前置清零是纯冗余双写。lm >= ceil(64*nb/k) = total 恒成立故无越界。
    split_b2(b, G, nb, F.k, F.lm);
    if (F.path == 3) {
        const u32 m = F.ts / 3;
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)G, m);
        fft::difRec((fft::cpx*)G,                 m, 0);
        fft::difRec((fft::cpx*)(G + 2*(size_t)m), m, 0);
        fft::difRec((fft::cpx*)(G + 4*(size_t)m), m, 0);
    } else if (F.path == 5) {
        const u32 m = F.ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)G, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(G + 2*(size_t)m*i), m, 0);
    } else {
        fft::resize(F.ts);
        fft::difRec((fft::cpx*)G, F.ts, 0);
    }
    F.ok = true;
}
static void fm_mul(const FixedFFT& F, double* G, const u64* a, int na, u64* c) {
    split_b2(a, FB, na, F.k, F.lm);   // v18-P2b: 冗余前置 memset 已删 (见 fm_prep 注释)
    if (F.path == 3) {
        const u32 m = F.ts / 3;
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)FB, m);
        fft::difRec((fft::cpx*)FB,                 m, 0);
        fft::difRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::difRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)G, m, 3);
        fft::ditRec((fft::cpx*)FB,                 m, 0);
        fft::ditRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::ditRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        fft::idit3StageR((fft::cpx*)FB, m);
    } else if (F.path == 5) {
        const u32 m = F.ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)FB, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)G, m, 5);
        for (int i = 0; i < 5; ++i) fft::ditRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::idit5StageR((fft::cpx*)FB, m);
    } else {
        fft::resize(F.ts);
        fft::difRec((fft::cpx*)FB, F.ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)G, F.ts);
        fft::ditRec((fft::cpx*)FB, F.ts, 0);
    }
    merge_b2(c, FB, F.u, F.k);
}

// ============================ 环形固定乘数 (mod B^mc - 1) ============================
// Barrett 块循环第二个乘法 P = qhat*BS 之后立刻做 Z -= P, 且 R = Z-P < 8*B^n。
// 故只需 P mod (B^mc-1) (mc >= n+2) 即可唯一定出 R —— 环形卷积长度 = 64*mc/k, 约为线性
// 长度 128n/k 的一半 (HALF 尺寸), 这是 DEC 相对 HEX 的核心算法杠杆。
//   正确性: R = Z - P ≡ (Zlo + Zhi) - cyc  (mod B^mc-1), 且 0 <= R < B^(n+1) <= B^(mc-1) < B^mc-1
//           => mc-limb 环内的代表元唯一 (仅 R==0 时可能取到 all-ones, 由 out[mc-1]!=0 判别)。
//   精度: 环形卷积输出系数 = 至多 d = ceil(64n/k) 项 digit 积之和 (操作数只有 n limb 非零),
//         判据 d*2^(2k) <= 2^47, 与已验证线性路径 (lm/2)*2^(2k) <= 2^47 同安全级。
struct CycFFT { u32 lm, ts; int k, path, mc; bool ok; };
static CycFFT CY2;
static bool g_cyc = true;                  // CYC=0 -> 退回线性 fm_mul

// 选 (L, k, mc): L 为最小合法 FFT 档, k | 64mc, mc = L*k/64 >= n+2, 精度 d*2^(2k) <= 2^47
static bool pick_cyclic(int n, u32& Lo, int& ko, int& mco) {
    const int nmin = n + 2;
    for (int j = 5; j < 25; ++j) {
        const u32 p = 1u << j;
        u32 cand[3] = { p, (p % 4 == 0) ? (p / 4 * 3) : 0, (p % 8 == 0) ? (p / 8 * 5) : 0 };
        // 同一 octave 内按 1.0 / 1.25 / 1.5 升序: p, 5p/8... 注意 p 自身最小, 其余属更高 octave 的细分档
        // 这里逐 octave 只需检查 p 与上一 octave 细分出的 3p/4, 5p/8 —— 统一排序后取首个可行
        u32 srt[3];
        int ns = 0;
        for (int i = 0; i < 3; ++i) if (cand[i] >= 32) srt[ns++] = cand[i];
        for (int i = 1; i < ns; ++i) for (int t = i; t > 0 && srt[t] < srt[t-1]; --t) { u32 s = srt[t]; srt[t] = srt[t-1]; srt[t-1] = s; }
        for (int i = 0; i < ns; ++i) {
            const u32 L = srt[i];
            for (int k = 19; k >= 8; --k) {
                if (((size_t)L * k) % 64) continue;
                const int mc = (int)((size_t)L * k / 64);
                if (mc < nmin) continue;
                const size_t d = ((size_t)64 * n + k - 1) / k;         // 单操作数非零 digit 数
                if (d > ((size_t)1 << 47) >> (2 * k)) continue;        // d*2^(2k) <= 2^47
                Lo = L; ko = k; mco = mc; return true;
            }
        }
    }
    return false;
}

static void cyc_prep(CycFFT& F, double* G, const u64* b, int nb, int n) {
    F.ok = false;
    if (nb <= MULBF_MAX) return;
    u32 L; int k, mc;
    if (!pick_cyclic(n, L, k, mc)) return;
    if (L > FMCAP) return;
    F.lm = L; F.ts = L >> 1; F.k = k; F.mc = mc;
    F.path = (L % 3 == 0) ? 3 : ((L % 5 == 0) ? 5 : 2);
    if (F.ts / (F.path == 3 ? 3u : (F.path == 5 ? 5u : 1u)) < 16) return;   // pointwise_blk 护栏
    // v18-P2b: 冗余前置 memset 已删。L = 64*mc/k 且 mc >= n+2 >= nb+2 => L > ceil(64*nb/k) = total。
    // v24: zero-hi。invertappr 用它做 xh (nb=h≈n/2) 的固定变换时 total <= L/2, 高半整块为零,
    //   顶层 radix-4 可只读下半 (同 mul_fft 的 v13 优化)。Barrett 用它做 BS(nb=n=mc) 时 total=L 不命中。
    const bool deep = (F.path == 2) && F.ts > (1u << FFT_LEAF_LOG);
    const size_t half = (size_t)L >> 1;
    const size_t tb = split_b2(b, G, nb, k, deep ? half : (size_t)L);
    const bool hiB = deep && tb <= half;
    if (deep && !hiB && tb < L) std::memset(G + tb, 0, ((size_t)L - tb) * 8);
    if (F.path == 3) {
        const u32 m = F.ts / 3;
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)G, m);
        fft::difRec((fft::cpx*)G,                 m, 0);
        fft::difRec((fft::cpx*)(G + 2*(size_t)m), m, 0);
        fft::difRec((fft::cpx*)(G + 4*(size_t)m), m, 0);
    } else if (F.path == 5) {
        const u32 m = F.ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)G, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(G + 2*(size_t)m*i), m, 0);
    } else {
        fft::resize(F.ts);
        if (hiB) fft::difRecZeroHi((fft::cpx*)G, F.ts); else fft::difRec((fft::cpx*)G, F.ts, 0);
    }
    F.ok = true;
}
// out[0..mc) = (a * b) mod (B^mc - 1)，b 的正变换已在 G 中
static void cyc_mul_fixed(const CycFFT& F, double* G, const u64* a, int na, u64* out) {
    // v18-P2b: 冗余前置 memset 已删。入参 a 已由 reduce_mod_bm1 折到 na=mc, 故 total = 64*mc/k = lm 恰好写满。
    // v24: zero-hi。Barrett 路径 na=mc => total=lm 不命中; invertappr 的 mul2 (a=Ehi, na≈n/2) 命中。
    const bool deep = (F.path == 2) && F.ts > (1u << FFT_LEAF_LOG);
    const size_t half = (size_t)F.lm >> 1;
    const size_t ta = split_b2(a, FB, na, F.k, deep ? half : (size_t)F.lm);
    const bool hiA = deep && ta <= half;
    if (deep && !hiA && ta < F.lm) std::memset(FB + ta, 0, ((size_t)F.lm - ta) * 8);
    if (F.path == 3) {
        const u32 m = F.ts / 3;
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)FB, m);
        fft::difRec((fft::cpx*)FB,                 m, 0);
        fft::difRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::difRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)G, m, 3);
        fft::ditRec((fft::cpx*)FB,                 m, 0);
        fft::ditRec((fft::cpx*)(FB + 2*(size_t)m), m, 0);
        fft::ditRec((fft::cpx*)(FB + 4*(size_t)m), m, 0);
        fft::idit3StageR((fft::cpx*)FB, m);
    } else if (F.path == 5) {
        const u32 m = F.ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)FB, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)G, m, 5);
        for (int i = 0; i < 5; ++i) fft::ditRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::idit5StageR((fft::cpx*)FB, m);
    } else {
        fft::resize(F.ts);
        if (hiA) fft::difRecZeroHi((fft::cpx*)FB, F.ts); else fft::difRec((fft::cpx*)FB, F.ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)G, F.ts);
        fft::ditRec((fft::cpx*)FB, F.ts, 0);
    }
    // digit -> limb: 环形卷积长度 L = 64*mc/k, 周期 (2^k)^L - 1 = B^mc - 1, 进位留在数内
    u128 tmp = 0; int w = 0; size_t jj = 0;
    for (int limb = 0; limb < F.mc; ++limb) {
        while (w < 64) { tmp += (u128)(u64)(FB[jj] + 0.5) << w; w += F.k; ++jj; }
        out[limb] = (u64)tmp; tmp >>= 64; w -= 64;
    }
    if (tmp) {                                   // B^mc ≡ 1 回绕
        u128 c = tmp;
        for (int i = 0; i < F.mc; ++i) { u128 s = (u128)out[i] + c; out[i] = (u64)s; c = s >> 64; if (!c) break; }
        if (c) { u128 s = (u128)out[0] + c; out[0] = (u64)s; }
    }
}

// Z[0..len) 模 (2^(64*mc)-1) 折成 zc[0..mc)。B=2^64, 故周期性 = (2^k)^L = 2^(64mc)。
// 因 B^mc ≡ 1, 高位块直接加回低位; 进位越顶则于位置 0 加 1 折回。全 1 即代表 0。
static void reduce_mod_bm1(const u64* Z, int len, int mc, u64* zc) {
    for (int i = 0; i < mc; ++i) zc[i] = 0;
    int b0 = len < mc ? len : mc;
    for (int i = 0; i < b0; ++i) zc[i] = Z[i];
    if (len > mc) {
        unsigned char c = 0;
        for (int i = 0; i < mc; ++i) {
            u64 add = (i < len - mc) ? Z[mc + i] : 0;
            c = _addcarry_u64(c, zc[i], add, (unsigned long long*)&zc[i]);
            if (i == mc - 1 && c) {                          // 进位越过最高 limb -> 位置 0 加 1 (B^mc≡1)
                unsigned char c2 = 1;
                for (int j = 0; j < mc; ++j) {
                    c2 = _addcarry_u64(c2, zc[j], (u64)0, (unsigned long long*)&zc[j]);
                    if (!c2) break;
                }
                // c2 仍 1 => zc 原全 1 -> 现全 0 (代表 0)
            }
        }
    }
    bool all1 = true;                                        // 全 1 = B^mc-1 ≡ 0
    for (int i = 0; i < mc; ++i) if (zc[i] != ~0ULL) { all1 = false; break; }
    if (all1) for (int i = 0; i < mc; ++i) zc[i] = 0;
}

// ==================== v22: 双操作数环形卷积 (mod B^mc - 1) ====================
// 动机 (VM66 callgrind, length_ratio_0): invertappr inclusive = 198.5M / 340.8M = 58.26%,
//   其中 95.8% 落在两次 mulg。第一次 mulg(d, n, xh, h) 结果 n+h ≈ 1.5n limb, 但下游只用到
//   T[h, n+4) —— 因为
//     W  = d*(B^n + xh*B^l) = T*B^l + d*B^n,   S = d + T[h..n+h)  (= W 的高 n limb)
//     E  = B^{2n} - W = G*B^n - T[0..h)*B^l,   G = B^n - S
//     Ehi= floor(E/B^n) = G - [T[0..h) != 0]
//   而 G < c*B^{l+1} (xh 已是 h-limb 下估且 -4), 故 G 只需低 l+4 limb, 即 T[h, h+l+4) = T[h, n+4)。
//   环形折叠 mod (B^mc-1) 把 T[mc, n+h) 加到 [0, n+h-mc), 干净区 = [mc-n, mc);
//   取 mc >= n+4 目标区即全干净。FFT 长度 ∝ mc ≈ n 而非 1.5n → 该步省 1/3,
//   递归 T(n)=T(n/2)+C1+C2 由 (1.53+1.0) 降到 (1.0+1.0) => invertappr 约 -21%。
// 精度: pick_cyclic 的判据 d=ceil(64*nmin/k), d*2^(2k) <= 2^47。环形输出系数项数
//   <= min(da, db) = ceil(64*nb/k) < d, 故判据保守有效 (与已验证的 cyc_mul_fixed 同安全级)。
// 返回 mc; 0 = 不适用 (尺寸/精度/长度不划算), 调用方回退 mulg。
static int cyc_mul_var(const u64* a, int na, const u64* b, int nb, int nmin, u64* out) {
    if (na <= MULBF_MAX || nb <= MULBF_MAX) return 0;
    u32 L; int k, mc;
    if (!pick_cyclic(nmin, L, k, mc)) return 0;             // mc >= nmin + 2
    if (L > LMMAX) return 0;
    { const int kl = pick_k((size_t)na + nb);               // 只有比线性全乘更短才划算
      if (L >= fft_len_for((size_t)na + nb, kl)) return 0; }
    const u32 ts = L >> 1;
    const int path = (L % 3 == 0) ? 3 : ((L % 5 == 0) ? 5 : 2);
    if (ts / (path == 3 ? 3u : (path == 5 ? 5u : 1u)) < 16) return 0;   // pointwise_blk 护栏
    split_b2(a, FB, na, k, L);                              // zlim=L: L = 64mc/k >= ceil(64na/k)
    split_b2(b, GB, nb, k, L);
    if (path == 3) {
        const u32 m = ts / 3;
        fft::resize(m);
        fft::dif3StageR((fft::cpx*)FB, m);
        for (int i = 0; i < 3; ++i) fft::difRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::dif3StageR((fft::cpx*)GB, m);
        for (int i = 0; i < 3; ++i) fft::difRec((fft::cpx*)(GB + 2*(size_t)m*i), m, 0);
        fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)GB, m, 3);
        for (int i = 0; i < 3; ++i) fft::ditRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::idit3StageR((fft::cpx*)FB, m);
    } else if (path == 5) {
        const u32 m = ts / 5;
        fft::resize(m);
        fft::dif5StageR((fft::cpx*)FB, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::dif5StageR((fft::cpx*)GB, m);
        for (int i = 0; i < 5; ++i) fft::difRec((fft::cpx*)(GB + 2*(size_t)m*i), m, 0);
        fft::pointwise_mixed((fft::cpx*)FB, (fft::cpx*)GB, m, 5);
        for (int i = 0; i < 5; ++i) fft::ditRec((fft::cpx*)(FB + 2*(size_t)m*i), m, 0);
        fft::idit5StageR((fft::cpx*)FB, m);
    } else {
        fft::resize(ts);
        fft::difRec((fft::cpx*)FB, ts, 0);
        fft::difRec((fft::cpx*)GB, ts, 0);
        fft::pointwise((fft::cpx*)FB, (fft::cpx*)GB, ts);
        fft::ditRec((fft::cpx*)FB, ts, 0);
    }
    u128 tmp = 0; int w = 0; size_t jj = 0;                 // digit -> limb, 进位留在环内
    for (int limb = 0; limb < mc; ++limb) {
        while (w < 64) { tmp += (u128)(u64)(FB[jj] + 0.5) << w; w += k; ++jj; }
        out[limb] = (u64)tmp; tmp >>= 64; w -= 64;
    }
    if (tmp) {                                              // B^mc ≡ 1 回绕
        u128 c = tmp;
        for (int i = 0; i < mc; ++i) { u128 s = (u128)out[i] + c; out[i] = (u64)s; c = s >> 64; if (!c) break; }
        if (c) { u128 s = (u128)out[0] + c; out[0] = (u64)s; }
    }
    return mc;
}
static bool g_invcyc = true;                 // INVCYC=0 -> invertappr 退回全长 mulg (归因/回退)
#ifdef INV_VERIFY
static const u64* g_vref = nullptr;          // INV_VERIFY: 完整 E 路径的参考 Ehi
#endif

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
#define INV_BASE 24                          // v21: psweep 实测 24 优于 48 (4 个监控 case 全面不升)
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
    invertappr(d + l, h, xh);             // xh ~ B^{2h}/d_hi - B^h  (下估)
    {                                            // xh -= 4  (保证 E >= 0)
        u64 bw = 4;
        for (int i = 0; i < h && bw; ++i) { u64 cur = xh[i]; xh[i] = cur - bw; bw = (cur < bw); }
        if (bw) std::memset(xh, 0, (size_t)h * 8);
    }
    // ---- v22: T = d*xh, 下游只需 T[h, n+4) (见 cyc_mul_var 头部推导) ----
    //   Ehi = G - [T[0..h)!=0], G = B^n - (d + T[h..n+h))。固定取 Ehi = G-1:
    //   T[0..h)==0 只在 d*xh ≡ 0 mod B^h 时发生, 此时真值 Ehi=G, 我们给 G-1 偏小 1 ->
    //   v 偏小 O(1) -> 仍满足本函数契约 "2^(64n)+v <= floor(2^(128n)/d)" 的下估方向。
    const int jg = l + 4;                        // Ehi 有效 ~l+1 limb, 留 3 limb 冗余
    u64* Eh = wp; wp += jg + 2;                  // 先存 S, 后就地变 Ehi
    u64* T = wp;
    // ---- v23: 两次乘法 (d*xh, Ehi*xh) 共用 xh 的正变换 ----
    //   mul1 = d*xh  mod (B^mc-1), mc >= n+4  -> 目标 T[h,n+4) 干净 (v22 已验证)
    //   mul2 = Ehi*xh, ne+h <= (l+4)+h = n+4 <= mc -> 环形无回绕 = 精确全乘 (逐 limb 等同 mulg)
    //   两者 (L,k,mc) 完全一致 => DFT(xh) 只算一次: 6 次变换降到 5 次 (-16.7%)
    CycFFT CI; CI.ok = false;
    int mcv = 0;
    if (g_invcyc && h > MULBF_MAX) {
        u32 L_; int k_, mc_;
        const int kl_ = pick_k((size_t)n + h);
        if (pick_cyclic(n + 2, L_, k_, mc_) && L_ <= FMCAP
            && L_ < fft_len_for((size_t)n + h, kl_)) {         // 仅当比线性全乘更短才走
            cyc_prep(CI, FMG4, xh, h, n + 2);                  // 固定乘数 xh 的正变换 (复用 2 次)
            if (CI.ok) { mcv = CI.mc; cyc_mul_fixed(CI, FMG4, d, n, T); }
        }
    }
    if (mcv) wp += mcv + 8;                      // mc >= n+4 => T[h, n+4) 干净
    else { wp += n + h + 2; mulg(d, n, xh, h, T); }
#ifdef INV_VERIFY
    {   // 对照: 完整 W/E 路径, 校验 (a) T 目标区一致 (b) Ehi 高位确为 0 (c) Ehi 低位一致
        u64* Tr = wp; wp += (size_t)n + h + 2;
        mulg(d, n, xh, h, Tr);
        for (int i = 0; i < jg; ++i) if (Tr[h + i] != T[h + i]) {
            fprintf(stderr, "INVV_T n=%d i=%d ref=%llx got=%llx mc=%d\n", n, i,
                    (unsigned long long)Tr[h + i], (unsigned long long)T[h + i], mcv); abort(); }
        u64* Wv = wp; wp += 2 * n + 2;
        std::memset(Wv, 0, (size_t)l * 8);
        std::memcpy(Wv + l, Tr, (size_t)(n + h) * 8);
        { unsigned char c = 0; for (int i = 0; i < n; ++i)
            c = _addcarry_u64(c, Wv[n + i], d[i], (unsigned long long*)&Wv[n + i]); }
        { unsigned char c = 1; for (int i = 0; i < 2 * n; ++i)
            c = _addcarry_u64(c, ~Wv[i], 0ULL, (unsigned long long*)&Wv[i]); }
        for (int i = jg; i < n; ++i) if (Wv[n + i]) {
            fprintf(stderr, "INVV_HI n=%d i=%d jg=%d val=%llx\n", n, i, jg,
                    (unsigned long long)Wv[n + i]); abort(); }
        g_vref = Wv + n;                         // 供下方低位比对
    }
#endif
    {   // S = (d + T[h..n+h)) mod B^jg
        unsigned char c = 0;
        for (int i = 0; i < jg; ++i)
            c = _addcarry_u64(c, d[i], T[h + i], (unsigned long long*)&Eh[i]);
    }
    {   // G = B^jg - S (S!=0) 否则 0; 再 -1 得 Ehi
        bool szero = true;
        for (int i = 0; i < jg; ++i) if (Eh[i]) { szero = false; break; }
        if (szero) { for (int i = 0; i < jg; ++i) Eh[i] = 0; }
        else {
            unsigned char c = 1;
            for (int i = 0; i < jg; ++i) c = _addcarry_u64(c, ~Eh[i], 0ULL, (unsigned long long*)&Eh[i]);
            for (int i = 0; i < jg; ++i) if (Eh[i]--) break;      // G >= 1 保证不下溢
        }
    }
#ifdef INV_VERIFY
    for (int i = 0; i < jg; ++i) if (g_vref[i] != Eh[i]) {
        fprintf(stderr, "INVV_EHI n=%d i=%d ref=%llx got=%llx mc=%d\n", n, i,
                (unsigned long long)g_vref[i], (unsigned long long)Eh[i], mcv); abort(); }
#endif
    const u64* Ehi = Eh;                         // floor(E / B^n), 有效 ~l+1 limbs
    int ne = jg;
    while (ne > 0 && Ehi[ne - 1] == 0) --ne;
    // v = xh*B^l + Ehi + floor(Ehi*xh / B^h)
    std::memset(v, 0, (size_t)n * 8);
    std::memcpy(v + l, xh, (size_t)h * 8);
    if (ne > 0) {
        u64* P = wp;
        // v24: mul2 独立判据。cyclic 省一次正变换 (复用 xh 谱), 但环形档 L 受 (L*k)%64==0 约束,
        //   可能比线性 fft_len_for(ne+h) 更长; 长度相等时 cyclic 必胜 (故用 <=), 更长则回退 mulg。
        //   (v23 未加此判据 -> length_ratio_4/5 回归 +0.40%/+1.18%)
        //   阈值 M2N/M2D: 允许 cyclic 档最长为线性档的 M2N/M2D 倍 (少一次正变换可换更长的档)。
#ifndef M2N
#define M2N 1
#define M2D 1
#endif
        bool use2 = false;
        if (mcv) { const int kl2 = pick_k((size_t)ne + h);
                   use2 = ((u64)CI.lm * M2D <= (u64)fft_len_for((size_t)ne + h, kl2) * M2N); }
        if (use2) {
            wp += mcv + 8;
            cyc_mul_fixed(CI, FMG4, Ehi, ne, P); // 复用 xh 正变换; ne+h <= mc => 精确全乘
#ifdef INV_VERIFY
            {   u64* Pr = wp; wp += (size_t)ne + h + 2;
                mulg(Ehi, ne, xh, h, Pr);
                for (int i = 0; i < ne + h; ++i) if (Pr[i] != P[i]) {
                    fprintf(stderr, "INVV_P n=%d i=%d ref=%llx got=%llx mc=%d\n", n, i,
                            (unsigned long long)Pr[i], (unsigned long long)P[i], mcv); abort(); }
                for (int i = ne + h; i < mcv; ++i) if (P[i]) {
                    fprintf(stderr, "INVV_PHI n=%d i=%d val=%llx\n", n, i,
                            (unsigned long long)P[i]); abort(); }
                wp = Pr; }
#endif
        } else { wp += ne + h + 2; mulg(Ehi, ne, xh, h, P); }
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
        cyc_prep(CY2, FMG3, BS, n, n);                     // 环形固定乘数 BS 的正变换
        // v19-B: 先只规划 FF2 尺寸 (不做变换) 定 use_cyc; 走环形时 FF2 的正变换是纯浪费, 跳过。
        const bool p2ok = fm_plan(FF2, n, n);
        const bool use_cyc = g_cyc && CY2.ok && p2ok && CY2.lm < FF2.lm;
        if (!use_cyc) fm_prep(FF2, FMG2, BS, n, n);          // 仅线性路径才需要 BS 的正变换
        u64* Zc = use_cyc ? wp : nullptr;                  // 环形: Z 模 (B^mc-1) 的 mc-limb 缓冲
        if (use_cyc) wp += CY2.mc + 8;
        load_block(t - 1, Z + n);
        load_block(t - 2, Z);
        for (int i = t - 2; i >= 0; --i) {
            u64* Z1 = Z + n;                                // 高半 (= 上轮余数, < BS)
            if (FF1.ok) fm_mul(FF1, FMG1, Z1, n, P);        // P = Z1 * q2
            else mulg(Z1, n, q2, n, P);
            unsigned char c = 0;                            // qhat = Z1 + hi_n(Z1*q2)
            for (int k = 0; k < n; ++k)
                c = _addcarry_u64(c, Z1[k], P[n + k], (unsigned long long*)&Qi[k]);
            if (use_cyc) {
                // 环形第二乘法: P = (qhat*BS) mod (B^mc-1); R = (Z-P) mod (B^mc-1) = R
                // (R < B^n < B^mc-1 => 环内代表元唯一, 取低 n limb 即 R)
                reduce_mod_bm1(Z, 2 * n, CY2.mc, Zc);       // Zc = Z mod (B^mc-1)
                cyc_mul_fixed(CY2, FMG3, Qi, n, P);         // P = (qhat*BS) mod (B^mc-1) (复用 P 缓冲)
                unsigned char b3 = 0;                       // Zc -= P   (mc limbs) mod (B^mc-1)
                for (int k = 0; k < CY2.mc; ++k)
                    b3 = _subborrow_u64(b3, Zc[k], P[k], (unsigned long long*)&Zc[k]);
                if (b3) {                                   // 借位 -> + (B^mc-1) = 全 1
                    unsigned char b4 = 0;
                    for (int k = 0; k < CY2.mc; ++k)
                        b4 = _addcarry_u64(b4, Zc[k], ~0ULL, (unsigned long long*)&Zc[k]);
                }
                bool all1 = true;                           // 全 1 表示 R=0
                for (int k = 0; k < CY2.mc; ++k) if (Zc[k] != ~0ULL) { all1 = false; break; }
                std::memset(Z, 0, (size_t)2 * n * 8);
                if (all1) { /* R = 0 */ }
                else { for (int k = 0; k <= n && k < CY2.mc; ++k) Z[k] = Zc[k]; }  // 预修正 S 可达 8*B^n, 需 n+1 limb
            } else {
                if (FF2.ok) fm_mul(FF2, FMG2, Qi, n, P);    // P = qhat * BS
                else mulg(Qi, n, BS, n, P);
                unsigned char br = 0;                       // Z -= P   (2n limbs)
                for (int k = 0; k < 2 * n; ++k)
                    br = _subborrow_u64(br, Z[k], P[k], (unsigned long long*)&Z[k]);
            }
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
        u64 c = 0;   // 进位宽度可达 2^sigma-1 bits, 必须用 u64 (sigma>=9 时 unsigned char 会截断)
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
    // 算法开关 (A/B 归因用): FMT=1 -> fm_mul 用混合档(默认); FMT=0 -> 退回旧纯 2 幂.
    //                        CYC=1 -> bz_divide 第二处乘法用环形固定乘数(待接线); CYC=0 -> 线性 fm_mul.
    { const char* e = getenv("FMT"); g_fmt = e ? (atoi(e) != 0) : true; }
    { const char* e = getenv("BZALL"); g_bzall = e ? (atoi(e) != 0) : false; }
    { const char* e = getenv("CYC"); g_cyc = e ? (atoi(e) != 0) : true; }
    { const char* e = getenv("INVCYC"); g_invcyc = e ? (atoi(e) != 0) : true; }
    { const char* e = getenv("SR"); g_sr = e ? (atoi(e) != 0) : false; }
    { const char* e = getenv("SRREV"); if (e) g_sr_rev = atoi(e); }
    if (g_sr) fft::sr_init_pi();   // 预计算置换缓存, 防止处理期 get_pi 改写全局 twbase
    if (getenv("SRCMP")) { return fft::sr_cmp_main(); }
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
    // v27-OUTfix: 每 query 立即刷出 (循环顶部刷出上一个 query), 使 outbuf 只容单 query 输出,
    // 多 query 累计不会撑爆 outbuf; 所有分支 (小数字快路径/nb==1/a<b/bigint) 因此均被正确刷出。
    auto flush_out = [&]() {
        long off = 0, nn = (long)(out - outbuf);
        while (off < nn) {
            long w = write(1, outbuf + off, nn - off);
            if (w <= 0) break;
            off += w;
        }
        out = outbuf;
    };

    for (u32 t = 0; t < T; ++t) {
        if (t > 0) flush_out();   // 刷出上一个 query 的输出
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
        // v19-A 【正确性修复】newton_divide 的 q_est 修正轮数 ~ E0 * an/B^(2n)
        //   = E0 * 2^(abits - bbits - 64*nb)  (an = A<<sigma, sigma = 64*nb - bbits), 而修正循环上限 32。
        //   旧判据 na <= 2*nb 是 limb 粒度: 归一化左移后 an 可越过 B^(2n) 达 2^63 倍, 32 轮修不完 -> 输出错。
        //   实测反例 medium_2 q8: na=498, nb=249 (na==2*nb 通过旧判据), abits=31820, bbits=15876,
        //   qbits=15944 超 64*nb+2 共 6 bit -> q_est 偏小 186, 32 轮后仍差 154, 余数多出 154*B (BZ 路径正确)。
        //   改为 bit 精确判据 + 8 bit 裕度: qbits <= 64*nb - 8 => E0*2^-8 < 1 => 修正轮数 <= 1。
        // v20 【路由】判据换成商比 t = qbits/bbits 的阈值 0.9。依据: VM66 callgrind 实测 crossover 扫描
        //   (12/33334-limb 组 t=0.05..0.95 逐点对比 newton vs BZ, 见 rscan{1,2,3}.log):
        //     nb=33334: t<=0.15 newton 快 1.7%, t>=0.18 BZ 快 5.5~17.0%
        //     nb=12000: t<=0.30 newton 全胜 (BZ 慢 2.8~4.0%)   <- FFT 档位锯齿, 非 nb 单调
        //     nb=4096 : t>=0.05 BZ 全胜 (newton 慢 2.5~18%)
        //     nb=1425 : t<=0.50 newton 快 2.0~3.0%, t>=0.60 BZ 快 4.3~8.3%
        //   crossover 被 FFT 长度量化切成锯齿, 无法用 nb 的平滑函数拟合。故只取所有尺寸都成立的
        //   保守区间 t > 0.9 改判 BZ: 命中 A≈B^2 形状 (length_ratio_0 / amax_1 / bzbound_*),
        //   不触碰 t 小的 case (power_0 / amax_0 / max_* / rnear_* / large_* 的主成本 query)。
        //   0.9*bbits <= 0.9*64*nb < 64*nb-8 (nb>=BZ_MIN=160), 故正确性裕度自动满足。
        const int64_t abits = (int64_t)64 * (na - 1) + (64 - (int)_lzcnt_u64(A[na - 1]));
        const int64_t bbits = (int64_t)64 * (nb - 1) + (64 - (int)_lzcnt_u64(B[nb - 1]));
        const bool newton_ok = (abits - bbits) * 10 <= bbits * 9;
        // 商 limb 数 = na-nb+1。极短商时 Knuth D 只跑几轮 O(nb)，远快于 BZ 的 M(nb)logn
        if (nb < BZ_MIN || na - nb + 1 <= KD_QMAX) {
            knuthD(A, na, B, nb, Qout, Rout);
        } else if (newton_ok && !g_bzall) {
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
    flush_out();   // 刷出最后一个 query 的输出
    return 0;
}
