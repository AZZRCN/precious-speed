// absSub microbenchmark: 现版 AVX2(store-forward stall) vs 纯标量 vs SWAR借位链
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>
#include <immintrin.h>

using Limb = uint16_t;
static constexpr Limb BASE = 10000;

template <typename T>
static inline T sub_half(T a, T b, T base, Limb &borrow)
{
    T d = a - b;
    borrow = (a < b);
    d += borrow ? base : 0;
    return d;
}

// ---------- V0: 现版 (AVX2 + store/scalar/load 往返) ----------
__attribute__((target("avx2")))
static bool sub_v0(const Limb *a, const Limb *b, Limb *out, size_t n)
{
    size_t i = 0;
    Limb borrow = 0;
    __m256i bias_vec = _mm256_set1_epi32(static_cast<int>(10000u | (10000u << 16)));
    for (; i + 15 < n; i += 16)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b + i));
        __m256i r = _mm256_sub_epi32(_mm256_add_epi32(va, bias_vec), vb);
        alignas(32) uint32_t tmp[8];
        _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
        uint32_t bw = borrow;
        for (int k = 0; k < 8; k++)
        {
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
        __m256i out_vec = _mm256_load_si256(reinterpret_cast<const __m256i *>(tmp));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(out + i), out_vec);
    }
    for (; i < n; i++)
        out[i] = sub_half<Limb>(a[i], b[i] + borrow, BASE, borrow);
    return borrow;
}

// ---------- V1: 纯标量 8 路展开 ----------
static bool sub_v1(const Limb *a, const Limb *b, Limb *out, size_t n)
{
    size_t i = 0;
    Limb borrow = 0;
    for (; i + 7 < n; i += 8)
    {
        out[i]     = sub_half<Limb>(a[i],     b[i]     + borrow, BASE, borrow);
        out[i + 1] = sub_half<Limb>(a[i + 1], b[i + 1] + borrow, BASE, borrow);
        out[i + 2] = sub_half<Limb>(a[i + 2], b[i + 2] + borrow, BASE, borrow);
        out[i + 3] = sub_half<Limb>(a[i + 3], b[i + 3] + borrow, BASE, borrow);
        out[i + 4] = sub_half<Limb>(a[i + 4], b[i + 4] + borrow, BASE, borrow);
        out[i + 5] = sub_half<Limb>(a[i + 5], b[i + 5] + borrow, BASE, borrow);
        out[i + 6] = sub_half<Limb>(a[i + 6], b[i + 6] + borrow, BASE, borrow);
        out[i + 7] = sub_half<Limb>(a[i + 7], b[i + 7] + borrow, BASE, borrow);
    }
    for (; i < n; i++)
        out[i] = sub_half<Limb>(a[i], b[i] + borrow, BASE, borrow);
    return borrow;
}

// ---------- V2: SWAR 借位链 (全向量, 无 store-forward) ----------
// G[i] = a[i]<b[i]  (generate borrow), P[i] = a[i]==b[i] (propagate)
// 借位链同构于二进制加法进位: A=G, B=G|P  =>  gen=A&B=G, prop=A^B=P
// carry_in_vec = (A+B+bin) ^ A ^ B
__attribute__((target("avx2,bmi2")))
static bool sub_v2(const Limb *a, const Limb *b, Limb *out, size_t n)
{
    size_t i = 0;
    uint32_t borrow = 0;
    const __m256i vbase = _mm256_set1_epi16((short)BASE);
    const __m256i vbit = _mm256_setr_epi16(1, 2, 4, 8, 16, 32, 64, 128,
                                           256, 512, 1024, 2048, 4096, 8192,
                                           (short)16384, (short)32768);
    for (; i + 15 < n; i += 16)
    {
        __m256i va = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(a + i));
        __m256i vb = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(b + i));
        // t = a + BASE - b  in [1, 2*BASE-1], 恒 < 32768 => signed cmp 可用
        __m256i t = _mm256_sub_epi16(_mm256_add_epi16(va, vbase), vb);
        __m256i gm = _mm256_cmpgt_epi16(vbase, t); // t < BASE  <=> a < b
        __m256i pm = _mm256_cmpeq_epi16(t, vbase); // t == BASE <=> a == b
        uint32_t G = _pext_u32((uint32_t)_mm256_movemask_epi8(gm), 0x55555555u);
        uint32_t P = _pext_u32((uint32_t)_mm256_movemask_epi8(pm), 0x55555555u);
        uint32_t A = G, B = G | P;
        uint32_t s = A + B + borrow;
        uint32_t cin = s ^ A ^ B;      // bit i = 进入 limb i 的借位
        uint32_t cout = (cin >> 1) | ((s >> 16) << 15); // bit i = limb i 的借出
        borrow = (s >> 16) & 1u;
        // mask -> vector
        __m256i vcin = _mm256_cmpeq_epi16(_mm256_and_si256(_mm256_set1_epi16((short)cin), vbit), vbit);
        __m256i vcout = _mm256_cmpeq_epi16(_mm256_and_si256(_mm256_set1_epi16((short)cout), vbit), vbit);
        __m256i bin_val = _mm256_srli_epi16(vcin, 15);              // 0/1
        __m256i sub_base = _mm256_andnot_si256(vcout, vbase);       // 无借出处 -BASE
        __m256i r = _mm256_sub_epi16(_mm256_sub_epi16(t, bin_val), sub_base);
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(out + i), r);
    }
    Limb bw = (Limb)borrow;
    for (; i < n; i++)
        out[i] = sub_half<Limb>(a[i], b[i] + bw, BASE, bw);
    return bw;
}

int main(int argc, char **argv)
{
    std::mt19937_64 rng(12345);
    // 覆盖两类数据: 随机(借位稀疏) + 近相等(借位链长, r_nearly_zero 场景)
    for (int mode = 0; mode < 2; mode++)
    {
        printf("=== mode %s ===\n", mode == 0 ? "random" : "near-equal(long borrow chain)");
        for (size_t n : {64u, 128u, 250u, 1000u, 4096u})
        {
            std::vector<Limb> a(n), b(n), o0(n), o1(n), o2(n);
            for (size_t i = 0; i < n; i++)
            {
                a[i] = rng() % BASE;
                if (mode == 0)
                    b[i] = rng() % (a[i] + 1);
                else
                    b[i] = a[i]; // 全等 => P 全 1, 借位链最长
            }
            if (mode == 1 && n) b[0] = a[0] ? a[0] - 1 : 0;
            a[n - 1] = BASE - 1; // 保证 a >= b

            bool r0 = sub_v0(a.data(), b.data(), o0.data(), n);
            bool r1 = sub_v1(a.data(), b.data(), o1.data(), n);
            bool r2 = sub_v2(a.data(), b.data(), o2.data(), n);
            bool ok = (r0 == r1) && (r1 == r2) &&
                      !memcmp(o0.data(), o1.data(), n * 2) &&
                      !memcmp(o1.data(), o2.data(), n * 2);

            size_t reps = 40000000 / n;
            auto bench = [&](auto fn) {
                // warmup
                for (size_t r = 0; r < reps / 8; r++) fn(a.data(), b.data(), o0.data(), n);
                auto t0 = std::chrono::steady_clock::now();
                for (size_t r = 0; r < reps; r++) fn(a.data(), b.data(), o0.data(), n);
                auto t1 = std::chrono::steady_clock::now();
                return std::chrono::duration<double, std::nano>(t1 - t0).count() / (reps * n);
            };
            double d0 = bench(sub_v0), d1 = bench(sub_v1), d2 = bench(sub_v2);
            printf("n=%5zu  correct=%s  V0(cur)=%.3f  V1(scalar)=%.3f  V2(swar)=%.3f  ns/limb"
                   "   | V2 vs V0 = %.2fx  V1 vs V0 = %.2fx\n",
                   n, ok ? "OK " : "BAD", d0, d1, d2, d0 / d2, d0 / d1);
        }
    }
    return 0;
}
