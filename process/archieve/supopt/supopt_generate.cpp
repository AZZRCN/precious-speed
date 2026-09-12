// =============================================================================
// supopt_generate.cpp
// =============================================================================
// 本文件列出 5 个需要生成新 SIMD 指令序列的优化点。
// 每个任务包含: 当前实现(CURRENT) + 参照实现(REFERENCE) + 待生成(TODO) + 测试(TEST)
//
// 项目背景:
//   - 大整数运算库, BASE=10^4, Limb=uint16_t, Limb2=uint32_t
//   - 三文件 add.cpp / mul.cpp / div.cpp 中以下函数均未优化
//   - LC 评测机: i7-11370H, 支持 AVX2 + AVX-512VL + FMA
//   - 编译选项: g++ -O3 -std=c++23 -march=native
//
// 类型约定:
//   Limb  = uint16_t  (值域 0-9999)
//   Limb2 = uint32_t  (两 limb 乘积不超过 9999*9999 ≈ 10^8, 远小于 uint32 上限)
//   BASE  = 10000
//   View  = 只读视图 {const Limb* ptr; size_t size;}
//   Span  = 可写视图 {Limb* ptr; size_t size;}
//
// 关键不变量:
//   - 双肢打包: 2 个 uint16 limb 打包到 1 个 uint32 (低16位=lo limb, 高16位=hi limb)
//   - 两 limb 相加最大 19998 < 65536, 不会进位到高 16 位
//   - absSub 用 bias 技巧: r = a + BASE - b, 范围 [1, 19999]
//
// =============================================================================

#include <immintrin.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <vector>
#include <algorithm>

// =============================================================================
// 公共类型定义 (与 add.cpp / mul.cpp / div.cpp 一致)
// =============================================================================

using Limb  = uint16_t;
using Limb2 = uint32_t;

struct View {
    const Limb* ptr;
    size_t size;
    View(const Limb* p, size_t s) : ptr(p), size(s) {}
    View(const struct Span& s);  // 定义在 Span 后
    const Limb& operator[](size_t i) const { return ptr[i]; }
};

struct Span {
    Limb* ptr;
    size_t size;
    Span(Limb* p, size_t s) : ptr(p), size(s) {}
    Limb& operator[](size_t i) { return ptr[i]; }
};

inline View::View(const Span& s) : ptr(s.ptr), size(s.size) {}

constexpr Limb BASE = 10000;
constexpr Limb HALF_BASE = BASE / 2;

// Barrett reduction for s/BASE (BASE=10000):
//   M = ceil(2^64 / 10000) = 0x68DB8BAC710CC
//   q = divBASE(s) = (uint64_t)((unsigned __int128)s * M >> 64) == s / 10000
//   r = s - q * BASE                                == s % 10000
// HINT_PREFETCH 宏 (与项目一致)
#define HINT_PREFETCH(addr, rw, loc) __builtin_prefetch((addr), (rw), (loc))

constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL;
static inline uint64_t divBASE(uint64_t s) {
    return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64);
}

template <typename T>
T add_half(T a, T b, T base, T &cf) {
    T r = a + b;
    cf = r >= base;
    T mask = T(0) - T(cf);
    return r - (base & mask);
}

template <typename T>
T sub_half(T a, T b, T base, T &bf) {
    bf = a < b;
    T mask = T(0) - T(bf);
    return a - b + (base & mask);
}

// =============================================================================
// 任务 #1: absSub_avx2 直接 store 优化
// =============================================================================
//
// 背景:
//   absAdd_avx2 已优化为"直接 store 到 out, 在 out 上做 carry 传播"，
//   消除了 tmp 中转 (store tmp → load tmp → store out)。
//   但 absSub_avx2 仍保留 tmp 中转，三文件一致。
//
// 瓶颈:
//   每次 16-limb 迭代多 1 次 store + 1 次 load (tmp 中转)
//
// 难点:
//   absSub 用 bias 技巧 (a + BASE - b), 结果范围 [1, 19999]
//   需确认直接 store 后, 在 out 上做 borrow 传播的正确性
//   注意: bias 加到 a 后, 每个 16-bit lane 范围 [1, 19999], 不会下溢
//
// 预期收益: 消除 1 store + 1 load, 提升 ~5-10%
//
// -----------------------------------------------------------------------------

// [CURRENT] 当前 absSub_avx2 (三文件一致, 有 tmp 中转)
static bool absSub_avx2_current(View in1, View in2, Span out) {
    assert(in1.size >= in2.size);
    size_t i = 0;
    Limb borrow = 0;
    __m256i bias_vec = _mm256_set1_epi32(static_cast<int>(10000u | (10000u << 16)));
    for (; i + 15 < in2.size; i += 16) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
        __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
        alignas(32) uint32_t tmp[8];
        _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
        // 串行 borrow 传播: 8 步, 每步处理 2 个 limb (lo/hi), 无分支掩码
        uint32_t bw = borrow;
        for (int k = 0; k < 8; k++) {
            uint32_t lo = tmp[k] & 0xFFFF;
            uint32_t hi = tmp[k] >> 16;
            lo -= bw;
            uint32_t blo = lo < 10000;
            lo -= (1u - blo) * 10000u;
            hi -= blo;
            uint32_t bhi = hi < 10000;
            hi -= (1u - bhi) * 10000u;
            bw = bhi;
            tmp[k] = lo | (hi << 16);
        }
        borrow = static_cast<Limb>(bw);
        __m256i out_vec = _mm256_load_si256(reinterpret_cast<__m256i *>(tmp));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);
    }
    // 标量尾部 (8 路展开)
    for (; i + 7 < in2.size; i += 8) {
        out[i]   = sub_half<Limb>(in1[i],   in2[i]   + borrow, BASE, borrow);
        out[i+1] = sub_half<Limb>(in1[i+1], in2[i+1] + borrow, BASE, borrow);
        out[i+2] = sub_half<Limb>(in1[i+2], in2[i+2] + borrow, BASE, borrow);
        out[i+3] = sub_half<Limb>(in1[i+3], in2[i+3] + borrow, BASE, borrow);
        out[i+4] = sub_half<Limb>(in1[i+4], in2[i+4] + borrow, BASE, borrow);
        out[i+5] = sub_half<Limb>(in1[i+5], in2[i+5] + borrow, BASE, borrow);
        out[i+6] = sub_half<Limb>(in1[i+6], in2[i+6] + borrow, BASE, borrow);
        out[i+7] = sub_half<Limb>(in1[i+7], in2[i+7] + borrow, BASE, borrow);
    }
    for (; i < in2.size; i++) {
        out[i] = sub_half<Limb>(in1[i], in2[i] + borrow, BASE, borrow);
    }
    for (; i < in1.size; i++) {
        out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
    }
    return borrow;
}

// [REFERENCE] absAdd_avx2 直接 store 模式 (已优化, 作为参照)
static bool absAdd_avx2_reference(View in1, View in2, Span out) {
    if (in1.size < in2.size) std::swap(in1, in2);
    size_t i = 0;
    Limb carry = 0;
    for (; i + 15 < in2.size; i += 16) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
        __m256i r = _mm256_add_epi32(a, b);
        // 直接 store 到 out, 省去 tmp 往返
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
        // 串行 carry 传播: 直接在 out 上操作
        uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);
        uint32_t c = carry;
        for (int k = 0; k < 8; k++) {
            uint32_t lo = p32[k] & 0xFFFF;
            uint32_t hi = p32[k] >> 16;
            lo += c;
            uint32_t clo = lo >= 10000;
            lo -= clo * 10000u;
            hi += clo;
            uint32_t chi = hi >= 10000;
            hi -= chi * 10000u;
            c = chi;
            p32[k] = lo | (hi << 16);
        }
        carry = static_cast<Limb>(c);
    }
    for (; i + 7 < in2.size; i += 8) {
        out[i]   = add_half<Limb>(in1[i],   in2[i]   + carry, BASE, carry);
        out[i+1] = add_half<Limb>(in1[i+1], in2[i+1] + carry, BASE, carry);
        out[i+2] = add_half<Limb>(in1[i+2], in2[i+2] + carry, BASE, carry);
        out[i+3] = add_half<Limb>(in1[i+3], in2[i+3] + carry, BASE, carry);
        out[i+4] = add_half<Limb>(in1[i+4], in2[i+4] + carry, BASE, carry);
        out[i+5] = add_half<Limb>(in1[i+5], in2[i+5] + carry, BASE, carry);
        out[i+6] = add_half<Limb>(in1[i+6], in2[i+6] + carry, BASE, carry);
        out[i+7] = add_half<Limb>(in1[i+7], in2[i+7] + carry, BASE, carry);
    }
    for (; i < in2.size; i++) {
        out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
    }
    for (; i < in1.size; i++) {
        if (carry == 0) break;
        out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
    }
    if (carry == 0 && i < in1.size && out.ptr != in1.ptr) {
        std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
    }
    return carry;
}

// [TODO] 生成新的 absSub_avx2, 消除 tmp 中转
// 目标: 直接 store 到 out, 在 out 上做 borrow 传播
static bool absSub_avx2_optimized(View in1, View in2, Span out) {
    // TODO: 参照 absAdd_avx2_reference 的直接 store 模式
    // 注意 absSub 的 bias 技巧: r = a + BASE - b, 范围 [1, 19999]
    // 需确认 borrow 传播在 out 上直接操作的正确性
    return absSub_avx2_current(in1, in2, out);  // 临时回退
}

// =============================================================================
// 任务 #2: absMul1 SIMD 向量化
// =============================================================================
//
// 背景:
//   absMul1 实现大整数乘以单 limb, 三文件均为纯标量实现
//   在 absDivBasicCore 中被调用 (qhat 估计), 是 DIV 的热点之一
//
// 瓶颈:
//   - 串行 carry 依赖 (每个 prod 依赖前一个 carry)
//   - 2 次除法/迭代 (% BASE 和 / BASE)
//   - 纯标量, 无 SIMD
//
// 可能的优化方向:
//   1. 用 Barrett 倒数乘法替代除法 (参考 divBASE)
//   2. 4-limb 块内串行, 块间预计算 (打破串行依赖)
//   3. 用 AVX2 打包 4 个 limb 同时处理
//
// 约束:
//   - Limb2 = uint32_t, prod = Limb2(in[i]) * x + carry, 最大 9999*9999+9999 ≈ 10^8, 不溢出 uint32
//   - 但 carry = prod / BASE 最大 9999, 加上下一个 prod 可能溢出? 需验证
//   - 输出 out[i] 值域 [0, 9999]
//
// 预期收益: DIV base case 热点, 间接影响 absInvNewton
//
// -----------------------------------------------------------------------------

// [CURRENT] 当前 absMul1 (三文件一致, 纯标量)
static Limb absMul1_current(View in, Limb x, Span out) {
    Limb carry = 0;
    for (size_t i = 0; i < in.size; i++) {
        Limb2 prod = Limb2(in[i]) * x + carry;  // uint32 = uint16 * uint16 + uint16
        out[i] = prod % BASE;   // 除法
        carry = prod / BASE;    // 除法
    }
    return carry;
}

// [REFERENCE] Barrett 倒数乘法 (来自 fftMul carry chain)
// divBASE(s) 用乘法 + 移位替代除法, q = s / 10000
// r = s - q * BASE  即 s % 10000

// [TODO] 生成 SIMD 优化的 absMul1
static Limb absMul1_optimized(View in, Limb x, Span out) {
    // TODO: 用 Barrett 倒数乘法替代除法
    // 可选: 4-limb 块并行 (块内串行 carry, 块间预计算)
    return absMul1_current(in, x, out);  // 临时回退
}

// =============================================================================
// 任务 #3: absDiv1 SIMD 向量化
// =============================================================================
//
// 背景:
//   absDiv1 实现大整数除以单 limb, 三文件均为纯标量
//   在 selfDivRem1 中使用, 用于大整数除以单 limb
//
// 瓶颈: 同 absMul1, 串行 rem 依赖 + 2 次除法/迭代
//
// 约束:
//   - prod = Limb2(in[i]) + Limb2(rem) * BASE, 最大 9999 + 9999*10000 ≈ 10^8, 不溢出 uint32
//   - 输出 out[i] 值域 [0, x-1], rem 值域 [0, x-1]
//   - 注意: 除数 x 是变量 (非编译期常量), Barrett 倒数需运行时计算
//
// -----------------------------------------------------------------------------

// [CURRENT] 当前 absDiv1 (三文件一致, 纯标量)
static Limb absDiv1_current(View in, Limb x, Span out) {
    Limb rem = 0;
    size_t i = in.size;
    while (i > 0) {
        i--;
        Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;  // uint32 = uint16 + uint16 * 10000
        out[i] = prod / x;   // 除法 (x 是变量)
        rem = prod % x;      // 除法
    }
    return rem;
}

// [TODO] 生成 SIMD 优化的 absDiv1
static Limb absDiv1_optimized(View in, Limb x, Span out) {
    // TODO: 运行时计算 Barrett 倒数 M = ceil(2^64 / x)
    // 用乘法 + 移位替代除法
    return absDiv1_current(in, x, out);  // 临时回退
}

// =============================================================================
// 任务 #4: fftMul carry chain 向量化
// =============================================================================
//
// 背景:
//   fftMul 的 carry chain 是 MUL 的核心瓶颈, 占 fftMul 40-50% 时间
//   当前用 8× 标量 Barrett 展开, 但 carry 串行依赖无法向量化
//
// 瓶颈:
//   carry-chain 串行依赖: s_{i+1} = q_i + v[i+1], q_i = divBASE(s_i)
//   每个 q 依赖前一个 carry, 无法并行
//
// 可能的优化方向:
//   方案 A: VPCLMULQDQ 加速进位合并 (需 AVX-512_VPCLMULQDQ)
//   方案 B: 并行进位 (carry-lookahead), 预计算多个 limb 局部和
//   方案 C: 多精度 Barrett, 一次处理 4 个 limb 的进位
//
// 数据特征:
//   v1[] 是 double 数组 (FFT 卷积结果), 值域 [0, N*10^8]
//   conv_len = len1 + len2 - 1 (卷积长度)
//   out[] 是 uint16_t (limb), 值域 [0, 9999]
//   carry 是 uint64_t, 值域 [0, ~10^8/N]
//
// 约束:
//   - s = carry + uint64_t(v1[i] + 0.5), 加 0.5 是四舍五入
//   - q = divBASE(s) = s / 10000 (Barrett 倒数乘法)
//   - out[i] = s - q * 10000 = s % 10000
//   - 安全范围: s <= 2200231879019999 (~2^51), 实际 s_max ~ 1.1e12 (2000x 余量)
//
// 预期收益: carry chain 占 fftMul 40-50% 时间, 理论提升空间大
//
// -----------------------------------------------------------------------------

// [CURRENT] 当前 fftMul carry chain (8× 标量 Barrett 展开)
static void fftMul_carry_current(const double *v1, size_t conv_len, Span out) {
    uint64_t carry = 0;
    size_t i = 0;
    for (; i + 7 < conv_len; i += 8) {
        HINT_PREFETCH(v1 + i + 16, 0, 0);
        HINT_PREFETCH(v1 + i + 24, 0, 0);
        // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
        uint64_t s0 = carry + uint64_t(v1[i]   + 0.5);
        uint64_t q0 = divBASE(s0);
        uint64_t s1 = q0 + uint64_t(v1[i+1] + 0.5);
        uint64_t q1 = divBASE(s1);
        uint64_t s2 = q1 + uint64_t(v1[i+2] + 0.5);
        uint64_t q2 = divBASE(s2);
        uint64_t s3 = q2 + uint64_t(v1[i+3] + 0.5);
        uint64_t q3 = divBASE(s3);
        uint64_t s4 = q3 + uint64_t(v1[i+4] + 0.5);
        uint64_t q4 = divBASE(s4);
        uint64_t s5 = q4 + uint64_t(v1[i+5] + 0.5);
        uint64_t q5 = divBASE(s5);
        uint64_t s6 = q5 + uint64_t(v1[i+6] + 0.5);
        uint64_t q6 = divBASE(s6);
        uint64_t s7 = q6 + uint64_t(v1[i+7] + 0.5);
        uint64_t q7 = divBASE(s7);
        out[i]   = Limb(s0 - q0 * BASE);
        out[i+1] = Limb(s1 - q1 * BASE);
        out[i+2] = Limb(s2 - q2 * BASE);
        out[i+3] = Limb(s3 - q3 * BASE);
        out[i+4] = Limb(s4 - q4 * BASE);
        out[i+5] = Limb(s5 - q5 * BASE);
        out[i+6] = Limb(s6 - q6 * BASE);
        out[i+7] = Limb(s7 - q7 * BASE);
        carry = q7;
    }
    for (; i < conv_len; i++) {
        carry += uint64_t(v1[i] + 0.5);
        uint64_t q = divBASE(carry);
        out[i] = Limb(carry - q * BASE);
        carry = q;
    }
    out[conv_len] = Limb(carry);
}

// [TODO] 生成向量化的 carry chain
static void fftMul_carry_optimized(const double *v1, size_t conv_len, Span out) {
    // TODO: 方案 A/B/C 任选其一
    // 方案 A: VPCLMULQDQ (需确认 ISA 支持)
    // 方案 B: carry-lookahead (预计算多个 limb 局部和, 一次性算进位)
    // 方案 C: 多精度 Barrett (4-limb 块)
    fftMul_carry_current(v1, conv_len, out);  // 临时回退
}

// =============================================================================
// 任务 #5: absDivBasicCore qhat 估计 SIMD 化
// =============================================================================
//
// 背景:
//   absDivBasicCore 是 absInvNewton 的 base case (k<=64), 也是 DIV 的热点
//   每次迭代用 qhat = high / divisor_high 估计商, 1 次除法
//
// 瓶颈:
//   - 每次迭代 1 次除法 (divisor_high 是变量)
//   - 串行依赖: dividend 不断更新 (dividend -= prod)
//
// 可能的优化方向:
//   - 批量预计算多个 qhat, 用 SIMD 并行处理
//   - 用 Barrett 倒数乘法替代除法 (divisor_high 运行时计算倒数)
//
// 约束:
//   - divisor_high >= HALF_BASE (5000), 归一化保证
//   - qhat 估计可能偏大 1-2, 需修正循环
//   - dividend 会被 absSub 修改, 难以完全并行
//
// 预期收益: absInvNewton base case 热点
//
// -----------------------------------------------------------------------------

// 辅助函数 (标量版, 供 absDivBasicCore 使用)
static int absCompare_scalar(View a, View b) {
    if (a.size != b.size) return a.size > b.size ? 1 : -1;
    for (size_t i = a.size; i > 0;) {
        i--;
        if (a.ptr[i] != b.ptr[i]) return a.ptr[i] > b.ptr[i] ? 1 : -1;
    }
    return 0;
}

static void absSub_scalar(Span a, View b, Span out) {
    Limb borrow = 0;
    for (size_t i = 0; i < b.size; i++) {
        out.ptr[i] = sub_half<Limb>(a.ptr[i], b.ptr[i] + borrow, BASE, borrow);
    }
    for (size_t i = b.size; i < a.size; i++) {
        out.ptr[i] = sub_half<Limb>(a.ptr[i], borrow, BASE, borrow);
    }
}

// [CURRENT] 当前 absDivBasicCore (三文件一致, 标量)
static void absDivBasicCore_current(Span dividend, View divisor, Span quotient) {
    if (dividend.size <= divisor.size) return;
    assert(divisor.size > 0);
    size_t len1 = dividend.size, len2 = divisor.size;
    Limb divisor_high = divisor[len2 - 1];
    assert(divisor_high >= HALF_BASE);
    size_t quot_idx = len1 - len2;

    thread_local std::vector<Limb> tprod;
    if (tprod.size() < len2 + 1) tprod.resize(len2 + 1);
    while (quot_idx > 0) {
        quot_idx--;
        len1 = quot_idx + len2;
        Limb high1 = dividend[len1], high2 = dividend[len1 - 1], qhat = 0;

        if (high1 >= divisor_high) {
            qhat = BASE - 1;
        } else {
            Limb2 high = Limb2(high1) * BASE + high2;
            qhat = high / divisor_high;  // 除法 (变量)
        }
        Span prod_span(tprod.data(), len2 + 1);
        // absMul1 调用 (任务 #2)
        Limb carry = absMul1_current(divisor, qhat, prod_span);
        prod_span[len2] = carry;
        if (prod_span[len2] == 0) prod_span.size = len2;

        Span dividend_span(dividend.ptr + quot_idx, len2 + 1);
        int count = 0;
        // 修正循环: qhat 可能偏大
        while (absCompare_scalar(prod_span, View(dividend_span)) > 0) {
            assert(count < 2);
            count++;
            absSub_scalar(prod_span, divisor, prod_span);
            qhat--;
        }
        absSub_scalar(dividend_span, prod_span, dividend_span);
        quotient[quot_idx] = qhat;
        dividend.size = len1;
    }
}

// [TODO] 生成 SIMD 优化的 absDivBasicCore
static void absDivBasicCore_optimized(Span dividend, View divisor, Span quotient) {
    // TODO: 用 Barrett 倒数乘法替代 qhat 除法
    // 可选: 批量预计算多个 qhat (受限于 dividend 串行更新)
    absDivBasicCore_current(dividend, divisor, quotient);  // 临时回退
}

// =============================================================================
// 测试框架
// =============================================================================

static std::vector<Limb> random_limbs(size_t n) {
    std::vector<Limb> v(n);
    for (size_t i = 0; i < n; i++) v[i] = rand() % BASE;
    // 确保最高位非零 (除非 n=0)
    if (n > 0 && v[n-1] == 0) v[n-1] = 1 + rand() % (BASE - 1);
    return v;
}

// 测试 absSub
bool test_absSub() {
    printf("=== Test absSub_avx2_optimized ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {1, 8, 16, 17, 32, 100, 1000, 10000};

    for (size_t s : sizes) {
        auto a = random_limbs(s);
        auto b = random_limbs(s);
        // 确保 a >= b
        for (size_t i = 0; i < s; i++) {
            if (a[i] < b[i]) std::swap(a[i], b[i]);
        }

        std::vector<Limb> out_cur(s), out_opt(s);
        View va(a.data(), s), vb(b.data(), s);
        Span sa(out_cur.data(), s), sb(out_opt.data(), s);

        bool bf_cur = absSub_avx2_current(va, vb, sa);
        bool bf_opt = absSub_avx2_optimized(va, vb, sb);

        if (bf_cur == bf_opt && out_cur == out_opt) {
            pass++;
        } else {
            fail++;
            printf("  FAIL s=%zu: borrow cur=%d opt=%d\n", s, bf_cur, bf_opt);
            // 显示前几个差异
            for (size_t i = 0; i < s && i < 8; i++) {
                if (out_cur[i] != out_opt[i]) {
                    printf("    limb[%zu]: cur=%u opt=%u\n", i, out_cur[i], out_opt[i]);
                    break;
                }
            }
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

// 测试 absMul1
bool test_absMul1() {
    printf("=== Test absMul1_optimized ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {1, 8, 16, 100, 1000, 10000};

    for (size_t s : sizes) {
        auto in = random_limbs(s);
        Limb x = 1 + rand() % (BASE - 1);  // 1-9999

        std::vector<Limb> out_cur(s), out_opt(s);
        View vin(in.data(), s);
        Span scur(out_cur.data(), s), sopt(out_opt.data(), s);

        Limb c_cur = absMul1_current(vin, x, scur);
        Limb c_opt = absMul1_optimized(vin, x, sopt);

        if (c_cur == c_opt && out_cur == out_opt) pass++;
        else {
            fail++;
            printf("  FAIL s=%zu x=%u: carry cur=%u opt=%u\n", s, x, c_cur, c_opt);
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

// 测试 absDiv1
bool test_absDiv1() {
    printf("=== Test absDiv1_optimized ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {1, 8, 16, 100, 1000, 10000};

    for (size_t s : sizes) {
        auto in = random_limbs(s);
        Limb x = 1 + rand() % (BASE - 1);

        std::vector<Limb> out_cur(s), out_opt(s);
        View vin(in.data(), s);
        Span scur(out_cur.data(), s), sopt(out_opt.data(), s);

        Limb r_cur = absDiv1_current(vin, x, scur);
        Limb r_opt = absDiv1_optimized(vin, x, sopt);

        if (r_cur == r_opt && out_cur == out_opt) pass++;
        else {
            fail++;
            printf("  FAIL s=%zu x=%u: rem cur=%u opt=%u\n", s, x, r_cur, r_opt);
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

// 测试 fftMul carry chain
bool test_fftMul_carry() {
    printf("=== Test fftMul_carry_optimized ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {8, 16, 100, 1000, 10000};

    for (size_t conv_len : sizes) {
        // 模拟 FFT 卷积结果 (double 数组)
        std::vector<double> v1(conv_len);
        for (size_t i = 0; i < conv_len; i++) {
            // 模拟卷积值: 随机 [0, 10^8]
            v1[i] = double(rand() % 100000000);
        }

        std::vector<Limb> out_cur(conv_len + 1), out_opt(conv_len + 1);
        Span scur(out_cur.data(), conv_len + 1);
        Span sopt(out_opt.data(), conv_len + 1);

        fftMul_carry_current(v1.data(), conv_len, scur);
        fftMul_carry_optimized(v1.data(), conv_len, sopt);

        if (out_cur == out_opt) pass++;
        else {
            fail++;
            printf("  FAIL conv_len=%zu\n", conv_len);
            for (size_t i = 0; i <= conv_len && i < 8; i++) {
                if (out_cur[i] != out_opt[i]) {
                    printf("    out[%zu]: cur=%u opt=%u (v1=%f)\n",
                           i, out_cur[i], out_opt[i], v1[i]);
                    break;
                }
            }
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

// =============================================================================
// main: 运行所有测试
// =============================================================================

int main() {
    printf("========================================\n");
    printf("SIMD 超优化任务测试\n");
    printf("========================================\n\n");

    bool ok = true;
    ok &= test_absSub();
    ok &= test_absMul1();
    ok &= test_absDiv1();
    ok &= test_fftMul_carry();
    // absDivBasicCore 测试较复杂, 省略

    printf("========================================\n");
    printf("总计: %s\n", ok ? "ALL PASS" : "SOME FAILED");
    printf("========================================\n");
    return ok ? 0 : 1;
}

// =============================================================================
// 性能测试模板 (可选)
// =============================================================================
//
// 对每个优化点, 可添加性能测试:
//
// template<typename Func>
// double bench(const char* name, Func f, int runs = 15, int loops = 10) {
//     double vals[15];
//     for (int r = 0; r < runs; r++) {
//         auto s = std::chrono::high_resolution_clock::now();
//         for (int l = 0; l < loops; l++) f();
//         auto e = std::chrono::high_resolution_clock::now();
//         vals[r] = std::chrono::duration<double, std::milli>(e - s).count() / loops;
//     }
//     std::sort(vals, vals + runs);
//     printf("%-30s %.4f ms\n", name, vals[runs/2]);
//     return vals[runs/2];
// }
//
// =============================================================================
