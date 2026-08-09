// carryProp microbench: 当前 4 段标量 (D39/D40) vs AVX2 4x4 转置版
//
// 背景: callgrind 显示 carryPropSeg 在 lri_00 占 13.0% Ir / lri_02 占 14.9%,
//       且 D1mr ~= 0 => 纯指令吞吐 bound, 减少指令数可直接兑现。
//
// 正确性: s < 2^45, double 精确表示整数到 2^53。
//   q = floor(s*1e-4) 的绝对误差 < 2^-19, 只有 s%1e4==0 时可能给出 q-1;
//   算出 r = s - q*1e4 后若 r >= 1e4 则 q++, r -= 1e4 (一次修正足够)。
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <random>
#include <immintrin.h>

using Limb = uint16_t;
static constexpr uint64_t BASE = 10000;
static constexpr uint64_t BARRETT_M = 0x68DB8BACULL * 0x10000ULL + 0x710CCULL; // 0x68DB8BAC710CC
static inline uint64_t divBASE(uint64_t s)
{
    return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64);
}

// ---------------- 当前实现 (D39/D40 carryPropSeg) ----------------
static uint64_t cp_scalar(const double *v, Limb *out, size_t n)
{
    constexpr size_t MIN_PAR = 2048;
    uint64_t carry = 0;
    size_t i = 0;
    if (n >= MIN_PAR)
    {
        const size_t seg = (n >> 2) & ~size_t(7);
        const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
        uint64_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        for (size_t k = 0; k < seg; ++k)
        {
            uint64_t s0 = c0 + uint64_t(v[k] + 0.5);
            uint64_t s1 = c1 + uint64_t(v[b1 + k] + 0.5);
            uint64_t s2 = c2 + uint64_t(v[b2 + k] + 0.5);
            uint64_t s3 = c3 + uint64_t(v[b3 + k] + 0.5);
            uint64_t q0 = divBASE(s0), q1 = divBASE(s1), q2 = divBASE(s2), q3 = divBASE(s3);
            out[k] = Limb(s0 - q0 * BASE);
            out[b1 + k] = Limb(s1 - q1 * BASE);
            out[b2 + k] = Limb(s2 - q2 * BASE);
            out[b3 + k] = Limb(s3 - q3 * BASE);
            c0 = q0; c1 = q1; c2 = q2; c3 = q3;
        }
        carry = c3;
        for (i = b3 + seg; i < n; ++i)
        {
            carry += uint64_t(v[i] + 0.5);
            uint64_t q = divBASE(carry);
            out[i] = Limb(carry - q * BASE);
            carry = q;
        }
        uint64_t ov = c0;
        for (size_t p = b1; ov > 0 && p < b2; ++p)
        { uint64_t s = uint64_t(out[p]) + ov; uint64_t q = divBASE(s); out[p] = Limb(s - q * BASE); ov = q; }
        ov += c1;
        for (size_t p = b2; ov > 0 && p < b3; ++p)
        { uint64_t s = uint64_t(out[p]) + ov; uint64_t q = divBASE(s); out[p] = Limb(s - q * BASE); ov = q; }
        ov += c2;
        for (size_t p = b3; ov > 0 && p < n; ++p)
        { uint64_t s = uint64_t(out[p]) + ov; uint64_t q = divBASE(s); out[p] = Limb(s - q * BASE); ov = q; }
        return carry + ov;
    }
    for (; i < n; ++i)
    {
        carry += uint64_t(v[i] + 0.5);
        uint64_t q = divBASE(carry);
        out[i] = Limb(carry - q * BASE);
        carry = q;
    }
    return carry;
}

// ---------------- AVX2 版: 4 段并行, 4x4 转置消除 gather ----------------
#define TRANSPOSE4_PD(r0, r1, r2, r3, t0, t1, t2, t3)                 \
    do {                                                              \
        __m256d _a = _mm256_unpacklo_pd(r0, r1);                      \
        __m256d _b = _mm256_unpackhi_pd(r0, r1);                      \
        __m256d _c = _mm256_unpacklo_pd(r2, r3);                      \
        __m256d _d = _mm256_unpackhi_pd(r2, r3);                      \
        t0 = _mm256_permute2f128_pd(_a, _c, 0x20);                    \
        t1 = _mm256_permute2f128_pd(_b, _d, 0x20);                    \
        t2 = _mm256_permute2f128_pd(_a, _c, 0x31);                    \
        t3 = _mm256_permute2f128_pd(_b, _d, 0x31);                    \
    } while (0)

static inline void cp_core4(__m256d rv, __m256d &carry, __m256d &r_out,
                            const __m256d vinv, const __m256d vbase, const __m256d vone)
{
    __m256d s = _mm256_add_pd(carry, rv);
    __m256d q = _mm256_floor_pd(_mm256_mul_pd(s, vinv));
    __m256d r = _mm256_fnmadd_pd(q, vbase, s);
    // 修正: r >= BASE -> q+1, r-BASE   (只可能差 1)
    __m256d m = _mm256_cmp_pd(r, vbase, _CMP_GE_OQ);
    q = _mm256_add_pd(q, _mm256_and_pd(m, vone));
    r = _mm256_sub_pd(r, _mm256_and_pd(m, vbase));
    // 修正: r < 0 -> q-1, r+BASE
    __m256d m2 = _mm256_cmp_pd(r, _mm256_setzero_pd(), _CMP_LT_OQ);
    q = _mm256_sub_pd(q, _mm256_and_pd(m2, vone));
    r = _mm256_add_pd(r, _mm256_and_pd(m2, vbase));
    carry = q;
    r_out = r;
}

static uint64_t cp_avx2(const double *v, Limb *out, size_t n)
{
    constexpr size_t MIN_PAR = 2048;
    if (n < MIN_PAR) return cp_scalar(v, out, n);

    const size_t seg = (n >> 2) & ~size_t(7);
    const size_t b1 = seg, b2 = seg * 2, b3 = seg * 3;
    const __m256d vinv = _mm256_set1_pd(1.0 / (double)BASE);
    const __m256d vbase = _mm256_set1_pd((double)BASE);
    const __m256d vone = _mm256_set1_pd(1.0);
    const __m256d vhalf = _mm256_set1_pd(0.5);
    __m256d carry = _mm256_setzero_pd();   // lane j = 段 j 的进位

    size_t k = 0;
    for (; k + 4 <= seg; k += 4)
    {
        __m256d r0 = _mm256_loadu_pd(v + k);
        __m256d r1 = _mm256_loadu_pd(v + b1 + k);
        __m256d r2 = _mm256_loadu_pd(v + b2 + k);
        __m256d r3 = _mm256_loadu_pd(v + b3 + k);
        // round-to-nearest via floor(x+0.5), 与标量 uint64_t(v+0.5) 一致
        r0 = _mm256_floor_pd(_mm256_add_pd(r0, vhalf));
        r1 = _mm256_floor_pd(_mm256_add_pd(r1, vhalf));
        r2 = _mm256_floor_pd(_mm256_add_pd(r2, vhalf));
        r3 = _mm256_floor_pd(_mm256_add_pd(r3, vhalf));
        __m256d t0, t1, t2, t3;
        TRANSPOSE4_PD(r0, r1, r2, r3, t0, t1, t2, t3);  // tj = [seg0[k+j],seg1[k+j],seg2[k+j],seg3[k+j]]
        __m256d o0, o1, o2, o3;
        cp_core4(t0, carry, o0, vinv, vbase, vone);
        cp_core4(t1, carry, o1, vinv, vbase, vone);
        cp_core4(t2, carry, o2, vinv, vbase, vone);
        cp_core4(t3, carry, o3, vinv, vbase, vone);
        // 转置回: uj = [segj[k],segj[k+1],segj[k+2],segj[k+3]]
        __m256d u0, u1, u2, u3;
        TRANSPOSE4_PD(o0, o1, o2, o3, u0, u1, u2, u3);
        __m128i i0 = _mm256_cvttpd_epi32(u0);
        __m128i i1 = _mm256_cvttpd_epi32(u1);
        __m128i i2 = _mm256_cvttpd_epi32(u2);
        __m128i i3 = _mm256_cvttpd_epi32(u3);
        _mm_storel_epi64((__m128i *)(out + k),      _mm_packus_epi32(i0, i0));
        _mm_storel_epi64((__m128i *)(out + b1 + k), _mm_packus_epi32(i1, i1));
        _mm_storel_epi64((__m128i *)(out + b2 + k), _mm_packus_epi32(i2, i2));
        _mm_storel_epi64((__m128i *)(out + b3 + k), _mm_packus_epi32(i3, i3));
    }
    alignas(32) double cbuf[4];
    _mm256_store_pd(cbuf, carry);
    uint64_t c0 = (uint64_t)cbuf[0], c1 = (uint64_t)cbuf[1],
             c2 = (uint64_t)cbuf[2], c3 = (uint64_t)cbuf[3];
    // seg 尾巴 (seg 是 8 的倍数, k 以 4 步进 => 恒好整除, 这里保险)
    for (; k < seg; ++k)
    {
        uint64_t s0 = c0 + uint64_t(v[k] + 0.5);
        uint64_t s1 = c1 + uint64_t(v[b1 + k] + 0.5);
        uint64_t s2 = c2 + uint64_t(v[b2 + k] + 0.5);
        uint64_t s3 = c3 + uint64_t(v[b3 + k] + 0.5);
        uint64_t q0 = divBASE(s0), q1 = divBASE(s1), q2 = divBASE(s2), q3 = divBASE(s3);
        out[k] = Limb(s0 - q0 * BASE);
        out[b1 + k] = Limb(s1 - q1 * BASE);
        out[b2 + k] = Limb(s2 - q2 * BASE);
        out[b3 + k] = Limb(s3 - q3 * BASE);
        c0 = q0; c1 = q1; c2 = q2; c3 = q3;
    }
    uint64_t carry_out = c3;
    for (size_t i = b3 + seg; i < n; ++i)
    {
        carry_out += uint64_t(v[i] + 0.5);
        uint64_t q = divBASE(carry_out);
        out[i] = Limb(carry_out - q * BASE);
        carry_out = q;
    }
    uint64_t ov = c0;
    for (size_t p = b1; ov > 0 && p < b2; ++p)
    { uint64_t s = uint64_t(out[p]) + ov; uint64_t q = divBASE(s); out[p] = Limb(s - q * BASE); ov = q; }
    ov += c1;
    for (size_t p = b2; ov > 0 && p < b3; ++p)
    { uint64_t s = uint64_t(out[p]) + ov; uint64_t q = divBASE(s); out[p] = Limb(s - q * BASE); ov = q; }
    ov += c2;
    for (size_t p = b3; ov > 0 && p < n; ++p)
    { uint64_t s = uint64_t(out[p]) + ov; uint64_t q = divBASE(s); out[p] = Limb(s - q * BASE); ov = q; }
    return carry_out + ov;
}

int main(int argc, char **argv)
{
    size_t n = (argc > 1) ? strtoull(argv[1], nullptr, 10) : 262144;
    int reps = (argc > 2) ? atoi(argv[2]) : 200;
    double vmax = (argc > 3) ? atof(argv[3]) : 2.6e13;

    std::vector<double> v(n);
    std::mt19937_64 rng(12345);
    std::uniform_real_distribution<double> dist(0.0, vmax);
    for (size_t i = 0; i < n; i++) v[i] = dist(rng);
    // 掺入边界值: 恰为 BASE 倍数 (触发 floor 边界), 以及 0
    for (size_t i = 0; i < n; i += 97) v[i] = (double)((uint64_t)(dist(rng) / BASE) * BASE);
    for (size_t i = 0; i < n; i += 1013) v[i] = 0.0;

    std::vector<Limb> o1(n + 8, 0), o2(n + 8, 0);

    uint64_t c1 = cp_scalar(v.data(), o1.data(), n);
    uint64_t c2 = cp_avx2(v.data(), o2.data(), n);
    size_t bad = 0;
    for (size_t i = 0; i < n; i++) if (o1[i] != o2[i]) { if (bad < 5) printf("  MISMATCH i=%zu %u vs %u\n", i, o1[i], o2[i]); bad++; }
    printf("correctness: carry %llu vs %llu   mismatches=%zu  %s\n",
           (unsigned long long)c1, (unsigned long long)c2, bad,
           (bad == 0 && c1 == c2) ? "OK" : "FAIL");

    auto bench = [&](const char *name, uint64_t (*fn)(const double *, Limb *, size_t), std::vector<Limb> &o) {
        for (int r = 0; r < 20; r++) fn(v.data(), o.data(), n);   // warm
        double best = 1e18;
        for (int r = 0; r < 5; r++)
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int j = 0; j < reps; j++) fn(v.data(), o.data(), n);
            double ms = std::chrono::duration<double, std::milli>(
                            std::chrono::high_resolution_clock::now() - t0).count();
            if (ms < best) best = ms;
        }
        double ns_per_limb = best * 1e6 / (double)reps / (double)n;
        printf("%-10s  best=%8.3f ms / %d reps   %6.3f ns/limb\n", name, best, reps, ns_per_limb);
        return ns_per_limb;
    };

    printf("n=%zu reps=%d vmax=%.3g\n", n, reps, vmax);
    double a = bench("scalar4", cp_scalar, o1);
    double b = bench("avx2", cp_avx2, o2);
    printf("speedup = %.3fx\n", a / b);
    return 0;
}
