// =============================================================================
// supopt_fast.cpp – 绝对速度版 (Zen 3)
// 所有 Barrett 除法使用"乘法回绕检查"修正（速度优先）
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

using Limb  = uint16_t;
using Limb2 = uint32_t;

struct View {
    const Limb* ptr;
    size_t size;
    View(const Limb* p, size_t s) : ptr(p), size(s) {}
    View(const struct Span& s);
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
constexpr uint64_t BARRETT_M64 = 0x68DB8BAC710CCULL;
constexpr uint32_t BARRETT_M32 = 429497;  // ceil(2^32 / 10000) = 429497 (原 429496 是 floor, 错误)

#define HINT_PREFETCH(addr, rw, loc) __builtin_prefetch((addr), (rw), (loc))

// 速度版 Barrett: 检查 q*BASE 是否超过 s
static inline uint64_t divBASE64(uint64_t s, uint64_t &q) {
    q = (uint64_t)((unsigned __int128)s * BARRETT_M64 >> 64);
    if (s < q * BASE) q--;
    return s - q * BASE;
}

static inline Limb divBASE32(uint64_t s, uint64_t &q) {
    q = (s * BARRETT_M32) >> 32;
    if (q * BASE > s) q--;
    return Limb(s - q * BASE);
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
// 任务 #1: absSub_avx2 直接 store
// =============================================================================
static bool absSub_avx2_optimized(View in1, View in2, Span out) {
    assert(in1.size >= in2.size);
    size_t i = 0;
    Limb borrow = 0;
    __m256i bias_vec = _mm256_set1_epi32(static_cast<int>(10000u | (10000u << 16)));
    for (; i + 15 < in2.size; i += 16) {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
        __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), r);
        uint32_t *p32 = reinterpret_cast<uint32_t *>(out.ptr + i);
        uint32_t bw = borrow;
        for (int k = 0; k < 8; k++) {
            uint32_t lo = p32[k] & 0xFFFF;
            uint32_t hi = p32[k] >> 16;
            lo -= bw;
            uint32_t blo = lo < 10000;
            lo -= (1u - blo) * 10000u;
            hi -= blo;
            uint32_t bhi = hi < 10000;
            hi -= (1u - bhi) * 10000u;
            bw = bhi;
            p32[k] = lo | (hi << 16);
        }
        borrow = static_cast<Limb>(bw);
    }
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

// =============================================================================
// 任务 #2: absMul1 SIMD (16-limb 并行 + 32 位 Barrett)
// =============================================================================
static Limb absMul1_optimized(View in, Limb x, Span out) {
    if (x == 0) {
        std::memset(out.ptr, 0, in.size * sizeof(Limb));
        return 0;
    }
    Limb carry = 0;
    size_t i = 0;
    const __m256i vx = _mm256_set1_epi32(x);
    for (; i + 15 < in.size; i += 16) {
        __m128i in_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i));
        __m256i a0 = _mm256_cvtepu16_epi32(in_lo);
        __m128i in_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i + 8));
        __m256i a1 = _mm256_cvtepu16_epi32(in_hi);
        __m256i p0 = _mm256_mullo_epi32(a0, vx);
        __m256i p1 = _mm256_mullo_epi32(a1, vx);
        alignas(32) uint32_t prods[16];
        _mm256_store_si256(reinterpret_cast<__m256i*>(prods),     p0);
        _mm256_store_si256(reinterpret_cast<__m256i*>(prods + 8), p1);
        for (int k = 0; k < 16; ++k) {
            uint64_t s = uint64_t(prods[k]) + carry;
            uint64_t q;
            Limb r = divBASE32(s, q);
            out[i + k] = r;
            carry = Limb(q);
        }
    }
    for (; i < in.size; ++i) {
        uint64_t s = uint64_t(in[i]) * x + carry;
        uint64_t q;
        Limb r = divBASE32(s, q);
        out[i] = r;
        carry = Limb(q);
    }
    return carry;
}

// =============================================================================
// 任务 #3: absDiv1 SIMD (SIMD 搬运 + 64 位 Barrett)
// =============================================================================
static Limb absDiv1_optimized(View in, Limb x, Span out) {
    if (x == 1) {
        if (out.ptr != in.ptr)
            std::memcpy(out.ptr, in.ptr, in.size * sizeof(Limb));
        return 0;
    }
    uint64_t M = (uint64_t)((((unsigned __int128)1 << 64) + x - 1) / x);
    Limb rem = 0;
    size_t i = in.size;
    while (i >= 8) {
        i -= 8;
        __m128i in16 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in.ptr + i));
        __m256i in32 = _mm256_cvtepu16_epi32(in16);
        alignas(32) uint32_t tmp_in[8], tmp_out[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(tmp_in), in32);
        for (int k = 7; k >= 0; --k) {
            uint64_t prod = uint64_t(tmp_in[k]) + uint64_t(rem) * BASE;
            uint64_t q = (uint64_t)((unsigned __int128)prod * M >> 64);
            if ((uint64_t)q * x > prod) q--;  // 速度版: 单次修正
            tmp_out[k] = (uint32_t)q;
            rem = Limb(prod - q * x);
        }
        __m256i r32 = _mm256_load_si256(reinterpret_cast<const __m256i*>(tmp_out));
        __m128i r16 = _mm256_cvtepi32_epi16(r32);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out.ptr + i), r16);
    }
    while (i > 0) {
        --i;
        uint64_t prod = uint64_t(in[i]) + uint64_t(rem) * BASE;
        uint64_t q = (uint64_t)((unsigned __int128)prod * M >> 64);
        if ((uint64_t)q * x > prod) q--;
        out[i] = Limb(q);
        rem = Limb(prod - q * x);
    }
    return rem;
}

// =============================================================================
// 任务 #4: fftMul carry chain (cvttpd_epi64 + 64 位 Barrett)
// 需要 AVX-512DQ, Zen 3 (Milan) 支持
// =============================================================================
#pragma GCC push_options
#pragma GCC target("avx512dq")
static void fftMul_carry_optimized(const double *v1, size_t conv_len, Span out) {
    uint64_t carry = 0;
    size_t i = 0;
    const __m256d vhalf = _mm256_set1_pd(0.5);
    for (; i + 7 < conv_len; i += 8) {
        HINT_PREFETCH(v1 + i + 16, 0, 0);
        HINT_PREFETCH(v1 + i + 24, 0, 0);
        __m256d vlow  = _mm256_loadu_pd(v1 + i);
        __m256d vhigh = _mm256_loadu_pd(v1 + i + 4);
        vlow  = _mm256_add_pd(vlow,  vhalf);
        vhigh = _mm256_add_pd(vhigh, vhalf);
        __m256i i64low  = _mm256_cvttpd_epi64(vlow);
        __m256i i64high = _mm256_cvttpd_epi64(vhigh);
        alignas(32) int64_t vals[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(vals),     i64low);
        _mm256_store_si256(reinterpret_cast<__m256i*>(vals + 4), i64high);
        for (int k = 0; k < 8; ++k) {
            // 防御: vals[k] 若为负(浮点误差), 截断为 0
            uint64_t uv = vals[k] < 0 ? 0 : (uint64_t)vals[k];
            uint64_t s = carry + uv;
            uint64_t q;
            uint64_t r = divBASE64(s, q);
            out[i + k] = Limb(r);
            carry = q;
        }
    }
    for (; i < conv_len; i++) {
        carry += uint64_t(v1[i] + 0.5);
        uint64_t q;
        uint64_t r = divBASE64(carry, q);
        out[i] = Limb(r);
        carry = q;
    }
    out[conv_len] = Limb(carry);
}
#pragma GCC pop_options

// =============================================================================
// 任务 #5: absDivBasicCore qhat 优化
// =============================================================================
static int absCompare_scalar(View a, View b) {
    if (a.size != b.size) return a.size > b.size ? 1 : -1;
    for (size_t i = a.size; i > 0;) {
        i--;
        if (a.ptr[i] != b.ptr[i]) return a.ptr[i] > b.ptr[i] ? 1 : -1;
    }
    return 0;
}

static void absDivBasicCore_optimized(Span dividend, View divisor, Span quotient) {
    if (dividend.size <= divisor.size) return;
    assert(divisor.size > 0);
    size_t len1 = dividend.size, len2 = divisor.size;
    Limb divisor_high = divisor[len2 - 1];
    assert(divisor_high >= HALF_BASE);
    uint64_t M = (uint64_t)((((unsigned __int128)1 << 64) + divisor_high - 1) / divisor_high);
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
            uint64_t high = (uint64_t)high1 * BASE + high2;
            uint64_t q = (uint64_t)(((unsigned __int128)high * M) >> 64);
            if (q >= BASE) q = BASE - 1;
            if ((uint64_t)q * divisor_high > high) q--;  // 速度版: 单次修正
            qhat = (Limb)q;
        }
        Span prod_span(tprod.data(), len2 + 1);
        Limb carry = absMul1_optimized(divisor, qhat, prod_span);
        prod_span[len2] = carry;
        if (prod_span[len2] == 0) prod_span.size = len2;
        Span dividend_span(dividend.ptr + quot_idx, len2 + 1);
        int count = 0;
        while (absCompare_scalar(prod_span, View(dividend_span)) > 0) {
            assert(count < 2);
            count++;
            absSub_avx2_optimized(prod_span, divisor, prod_span);
            qhat--;
        }
        absSub_avx2_optimized(dividend_span, prod_span, dividend_span);
        quotient[quot_idx] = qhat;
        dividend.size = len1;
    }
}

// =============================================================================
// 对照 current 函数
// =============================================================================
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

static Limb absMul1_current(View in, Limb x, Span out) {
    Limb carry = 0;
    for (size_t i = 0; i < in.size; i++) {
        Limb2 prod = Limb2(in[i]) * x + carry;
        out[i] = prod % BASE;
        carry = prod / BASE;
    }
    return carry;
}

static Limb absDiv1_current(View in, Limb x, Span out) {
    Limb rem = 0;
    size_t i = in.size;
    while (i > 0) {
        i--;
        Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;
        out[i] = prod / x;
        rem = prod % x;
    }
    return rem;
}

static void fftMul_carry_current(const double *v1, size_t conv_len, Span out) {
    uint64_t carry = 0;
    size_t i = 0;
    for (; i + 7 < conv_len; i += 8) {
        HINT_PREFETCH(v1 + i + 16, 0, 0);
        HINT_PREFETCH(v1 + i + 24, 0, 0);
        uint64_t s0 = carry + uint64_t(v1[i]   + 0.5);
        uint64_t q0 = (uint64_t)((unsigned __int128)s0 * BARRETT_M64 >> 64);
        uint64_t s1 = q0 + uint64_t(v1[i+1] + 0.5);
        uint64_t q1 = (uint64_t)((unsigned __int128)s1 * BARRETT_M64 >> 64);
        uint64_t s2 = q1 + uint64_t(v1[i+2] + 0.5);
        uint64_t q2 = (uint64_t)((unsigned __int128)s2 * BARRETT_M64 >> 64);
        uint64_t s3 = q2 + uint64_t(v1[i+3] + 0.5);
        uint64_t q3 = (uint64_t)((unsigned __int128)s3 * BARRETT_M64 >> 64);
        uint64_t s4 = q3 + uint64_t(v1[i+4] + 0.5);
        uint64_t q4 = (uint64_t)((unsigned __int128)s4 * BARRETT_M64 >> 64);
        uint64_t s5 = q4 + uint64_t(v1[i+5] + 0.5);
        uint64_t q5 = (uint64_t)((unsigned __int128)s5 * BARRETT_M64 >> 64);
        uint64_t s6 = q5 + uint64_t(v1[i+6] + 0.5);
        uint64_t q6 = (uint64_t)((unsigned __int128)s6 * BARRETT_M64 >> 64);
        uint64_t s7 = q6 + uint64_t(v1[i+7] + 0.5);
        uint64_t q7 = (uint64_t)((unsigned __int128)s7 * BARRETT_M64 >> 64);
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
        uint64_t q = (uint64_t)((unsigned __int128)carry * BARRETT_M64 >> 64);
        out[i] = Limb(carry - q * BASE);
        carry = q;
    }
    out[conv_len] = Limb(carry);
}

// =============================================================================
// 测试框架
// =============================================================================
static std::vector<Limb> random_limbs(size_t n) {
    std::vector<Limb> v(n);
    for (size_t i = 0; i < n; i++) v[i] = rand() % BASE;
    if (n > 0 && v[n-1] == 0) v[n-1] = 1 + rand() % (BASE - 1);
    return v;
}

// 整体 a >= b 的随机用例 (不逐位交换, 允许借位传播)
static std::vector<Limb> random_limbs_ge(const std::vector<Limb>& b) {
    std::vector<Limb> a(b.size());
    for (size_t i = 0; i < b.size(); i++) a[i] = rand() % BASE;
    if (b.size() > 0) {
        // 保证整体 a >= b: 高位加一个偏移
        if (a[b.size()-1] < b[b.size()-1]) a[b.size()-1] = b[b.size()-1];
    }
    return a;
}

bool test_absSub() {
    printf("=== Test absSub_avx2_optimized (借位传播) ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {1, 8, 16, 17, 32, 100, 1000, 10000};
    for (size_t s : sizes) {
        auto b = random_limbs(s);
        auto a = random_limbs_ge(b);  // 整体 a >= b, 允许个别 limb a[i] < b[i]
        std::vector<Limb> out_cur(s), out_opt(s);
        View va(a.data(), s), vb(b.data(), s);
        Span sa(out_cur.data(), s), sb(out_opt.data(), s);
        bool bf_cur = absSub_avx2_current(va, vb, sa);
        bool bf_opt = absSub_avx2_optimized(va, vb, sb);
        if (bf_cur == bf_opt && out_cur == out_opt) pass++;
        else {
            fail++;
            printf("  FAIL s=%zu: borrow cur=%d opt=%d\n", s, bf_cur, bf_opt);
            for (size_t i = 0; i < s && i < 8; i++) {
                if (out_cur[i] != out_opt[i]) {
                    printf("    limb[%zu]: cur=%u opt=%u (a=%u b=%u)\n",
                           i, out_cur[i], out_opt[i], a[i], b[i]);
                    break;
                }
            }
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

bool test_absMul1() {
    printf("=== Test absMul1_optimized ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {1, 8, 16, 100, 1000, 10000};
    for (size_t s : sizes) {
        auto in = random_limbs(s);
        Limb x = 1 + rand() % (BASE - 1);
        std::vector<Limb> out_cur(s), out_opt(s);
        View vin(in.data(), s);
        Span scur(out_cur.data(), s), sopt(out_opt.data(), s);
        Limb c_cur = absMul1_current(vin, x, scur);
        Limb c_opt = absMul1_optimized(vin, x, sopt);
        if (c_cur == c_opt && out_cur == out_opt) pass++;
        else {
            fail++;
            printf("  FAIL s=%zu x=%u: carry cur=%u opt=%u\n", s, x, c_cur, c_opt);
            for (size_t i = 0; i < s && i < 8; i++) {
                if (out_cur[i] != out_opt[i]) {
                    printf("    limb[%zu]: cur=%u opt=%u (in=%u)\n", i, out_cur[i], out_opt[i], in[i]);
                    break;
                }
            }
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

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
            for (size_t i = 0; i < s && i < 8; i++) {
                if (out_cur[i] != out_opt[i]) {
                    printf("    limb[%zu]: cur=%u opt=%u (in=%u)\n", i, out_cur[i], out_opt[i], in[i]);
                    break;
                }
            }
        }
    }
    printf("结果: %d PASS, %d FAIL\n\n", pass, fail);
    return fail == 0;
}

bool test_fftMul_carry() {
    printf("=== Test fftMul_carry_optimized ===\n");
    int pass = 0, fail = 0;
    size_t sizes[] = {8, 16, 100, 1000, 10000};
    for (size_t conv_len : sizes) {
        std::vector<double> v1(conv_len);
        for (size_t i = 0; i < conv_len; i++) {
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

int main() {
    printf("========================================\n");
    printf("SIMD 超优化任务测试 (绝对速度版)\n");
    printf("========================================\n\n");
    bool ok = true;
    ok &= test_absSub();
    ok &= test_absMul1();
    ok &= test_absDiv1();
    ok &= test_fftMul_carry();
    printf("========================================\n");
    printf("总计: %s\n", ok ? "ALL PASS" : "SOME FAILED");
    printf("========================================\n");
    return ok ? 0 : 1;
}
