// carrybench.cpp — IFFT 后进位传播链: 标量 8 路展开 vs SIMD+SWAR
// 目标: 验证 D27 改造的正确性与真实加速比 (est/Ir 看不出全部收益, 因为原版是 latency-bound)
//
// 算法 (SIMD 版):
//   A. v_i (double, <=2e13) 拆成 4 个 base-1e4 数字 d0..d3   (3 次无修正除法, 每次 3 条指令)
//   B. t_k = d0_k + d1_{k-1} + d2_{k-2} + d3_{k-3}  <= 3*9999+19 = 30016 < 32768  (跨 lane rotate+blend)
//   C. 转 u16; g = t/1e4 ∈[0,3] (3 次 cmpgt); u = t - g*1e4; w = u + g_{k-1} <= 10002
//   D. w 的进位只有 0/1 -> SWAR generate/propagate, 一条 32 位加法算 16 limb (D11 已验证)
//
// 编译: g++ -O2 -std=c++23 -march=x86-64-v3 -o carrybench carrybench.cpp
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <immintrin.h>

using Limb = uint16_t;
static constexpr uint64_t BASE = 10000;
static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL;
static inline uint64_t divBASE(uint64_t s) { return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }

// ---------------------------------------------------------------- 标量基线 (照抄 div_D25.cpp)
static uint64_t carry_scalar(const double *v, size_t n, Limb *out)
{
    uint64_t carry = 0;
    size_t i = 0;
    for (; i + 7 < n; i += 8)
    {
        __builtin_prefetch(v + i + 16, 0, 0);
        __builtin_prefetch(v + i + 24, 0, 0);
        uint64_t s0 = carry + uint64_t(v[i]   + 0.5); uint64_t q0 = divBASE(s0);
        uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);    uint64_t q1 = divBASE(s1);
        uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);    uint64_t q2 = divBASE(s2);
        uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);    uint64_t q3 = divBASE(s3);
        uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);    uint64_t q4 = divBASE(s4);
        uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);    uint64_t q5 = divBASE(s5);
        uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);    uint64_t q6 = divBASE(s6);
        uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);    uint64_t q7 = divBASE(s7);
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
    for (; i < n; i++)
    {
        carry += uint64_t(v[i] + 0.5);
        uint64_t q = divBASE(carry);
        out[i] = Limb(carry - q * BASE);
        carry = q;
    }
    return carry;
}

// ---------------------------------------------------------------- SIMD + SWAR
// INV4 = double(1e-4) 上抬 4 ulp, 保证 fl(w*INV4) > w/1e4 严格成立 => trunc 无需修正
static inline double make_inv4()
{
    double d = 1e-4;
    uint64_t u;
    std::memcpy(&u, &d, 8);
    u += 4;
    std::memcpy(&d, &u, 8);
    return d;
}

static uint64_t carry_simd(const double *v, size_t n, Limb *out)
{
    if (n < 64)
        return carry_scalar(v, n, out);

    const __m256d VINV  = _mm256_set1_pd(make_inv4());
    const __m256d V1E4  = _mm256_set1_pd(10000.0);
    const __m256d VHALF = _mm256_set1_pd(0.5);
    const __m256i C9999  = _mm256_set1_epi16(9999);
    const __m256i C19999 = _mm256_set1_epi16(19999);
    const __m256i C29999 = _mm256_set1_epi16((short)29999);
    const __m256i C1E4   = _mm256_set1_epi16((short)10000);
    const __m256i VBIT   = _mm256_setr_epi16(1, 2, 4, 8, 16, 32, 64, 128,
                                             256, 512, 1024, 2048, 4096, 8192, 16384, (short)32768);

    // 主循环处理 [0, m), 保证尾部 >= 16 个元素给标量吸收 pending 溢出
    size_t m = (n / 16) * 16;
    if (m + 16 > n)
        m = (m >= 16) ? m - 16 : 0;
    if (m == 0)
        return carry_scalar(v, n, out);

    __m256d pr1 = _mm256_setzero_pd(), pr2 = _mm256_setzero_pd(), pr3 = _mm256_setzero_pd();
    __m256i prevG = _mm256_setzero_si256();
    uint32_t cin = 0;
    // 主循环最后一轮的 d1/d2/d3 尾部值 (用于 pending)
    double lastD1[4] = {0, 0, 0, 0}, lastD2[4] = {0, 0, 0, 0}, lastD3[4] = {0, 0, 0, 0};

    for (size_t i = 0; i < m; i += 16)
    {
        __builtin_prefetch(v + i + 32, 0, 0);
        __builtin_prefetch(v + i + 40, 0, 0);
        __m256d T[4];
        for (int b = 0; b < 4; b++)
        {
            __m256d w = _mm256_loadu_pd(v + i + 4 * b);
            // round-to-nearest, 与标量 uint64_t(v+0.5) 等价 (v >= 0)
            w = _mm256_floor_pd(_mm256_add_pd(w, VHALF));
            // A: 3 次除 1e4 (无修正)
            __m256d q1 = _mm256_round_pd(_mm256_mul_pd(w, VINV),  _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
            __m256d d0 = _mm256_fnmadd_pd(q1, V1E4, w);
            __m256d q2 = _mm256_round_pd(_mm256_mul_pd(q1, VINV), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
            __m256d d1 = _mm256_fnmadd_pd(q2, V1E4, q1);
            __m256d q3 = _mm256_round_pd(_mm256_mul_pd(q2, VINV), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
            __m256d d2 = _mm256_fnmadd_pd(q3, V1E4, q2);
            __m256d d3 = q3;
            // B: 跨 lane rotate + blend + add
            __m256d r1 = _mm256_permute4x64_pd(d1, _MM_SHUFFLE(2, 1, 0, 3));
            __m256d s1 = _mm256_blend_pd(r1, pr1, 0x1);
            pr1 = r1;
            __m256d r2 = _mm256_permute4x64_pd(d2, _MM_SHUFFLE(1, 0, 3, 2));
            __m256d s2 = _mm256_blend_pd(r2, pr2, 0x3);
            pr2 = r2;
            __m256d r3 = _mm256_permute4x64_pd(d3, _MM_SHUFFLE(0, 3, 2, 1));
            __m256d s3 = _mm256_blend_pd(r3, pr3, 0x7);
            pr3 = r3;
            T[b] = _mm256_add_pd(_mm256_add_pd(d0, s1), _mm256_add_pd(s2, s3));
            if (i + 16 == m && b == 3)
            {
                _mm256_storeu_pd(lastD1, d1);
                _mm256_storeu_pd(lastD2, d2);
                _mm256_storeu_pd(lastD3, d3);
            }
        }
        // 转 u16: [i0 i1 i2 i3]
        __m128i i0 = _mm256_cvttpd_epi32(T[0]);
        __m128i i1 = _mm256_cvttpd_epi32(T[1]);
        __m128i i2 = _mm256_cvttpd_epi32(T[2]);
        __m128i i3 = _mm256_cvttpd_epi32(T[3]);
        __m256i a01 = _mm256_inserti128_si256(_mm256_castsi128_si256(i0), i1, 1);
        __m256i a23 = _mm256_inserti128_si256(_mm256_castsi128_si256(i2), i3, 1);
        __m256i pk  = _mm256_packus_epi32(a01, a23);
        __m256i Tv  = _mm256_permute4x64_epi64(pk, _MM_SHUFFLE(3, 1, 2, 0));
        // C: g = t/1e4 ∈ [0,3];  u = t - g*1e4;  w = u + g_{k-1}
        __m256i m1 = _mm256_cmpgt_epi16(Tv, C9999);
        __m256i m2 = _mm256_cmpgt_epi16(Tv, C19999);
        __m256i m3 = _mm256_cmpgt_epi16(Tv, C29999);
        __m256i ms = _mm256_add_epi16(_mm256_add_epi16(m1, m2), m3);   // = -g
        __m256i U  = _mm256_add_epi16(Tv, _mm256_mullo_epi16(ms, C1E4));
        __m256i G  = _mm256_sub_epi16(_mm256_setzero_si256(), ms);
        __m256i sh = _mm256_permute2x128_si256(prevG, G, 0x21);
        __m256i Gs = _mm256_alignr_epi8(G, sh, 14);
        prevG = G;
        __m256i W  = _mm256_add_epi16(U, Gs);                          // <= 10002
        // D: SWAR 0/1 进位链
        __m256i vg = _mm256_cmpgt_epi16(W, C9999);
        __m256i vp = _mm256_cmpeq_epi16(W, C9999);
        uint32_t G16 = _pext_u32((uint32_t)_mm256_movemask_epi8(vg), 0x55555555u);
        uint32_t P16 = _pext_u32((uint32_t)_mm256_movemask_epi8(vp), 0x55555555u);
        uint32_t A = G16, B = G16 | P16;
        uint32_t s = A + B + cin;
        uint32_t cb = s ^ A ^ B;                                       // bit k = carry INTO k
        cin = (cb >> 16) & 1u;
        __m256i vc  = _mm256_cmpeq_epi16(_mm256_and_si256(_mm256_set1_epi16((short)(cb & 0xFFFFu)), VBIT), VBIT);
        __m256i vcn = _mm256_cmpeq_epi16(_mm256_and_si256(_mm256_set1_epi16((short)((cb >> 1) & 0xFFFFu)), VBIT), VBIT);
        __m256i R = _mm256_sub_epi16(W, vc);                           // vc = -1 -> +1
        R = _mm256_sub_epi16(R, _mm256_and_si256(vcn, C1E4));
        _mm256_storeu_si256((__m256i *)(out + i), R);
    }

    // pending: 主循环末尾溢出到位置 m, m+1, m+2 的部分
    // 注意 C 阶段的 g_{m-1} 也溢出到位置 m, 必须计入
    alignas(32) uint16_t gbuf[16];
    _mm256_storeu_si256((__m256i *)gbuf, prevG);
    uint64_t p0 = (uint64_t)gbuf[15]
                + (uint64_t)lastD1[3] + (uint64_t)lastD2[2] + (uint64_t)lastD3[1];
    uint64_t p1 = (uint64_t)lastD2[3] + (uint64_t)lastD3[2];
    uint64_t p2 = (uint64_t)lastD3[3];
    uint64_t carry = (uint64_t)cin + p0 + p1 * 10000ULL + p2 * 100000000ULL;

    for (size_t i = m; i < n; i++)
    {
        carry += uint64_t(v[i] + 0.5);
        uint64_t q = divBASE(carry);
        out[i] = Limb(carry - q * BASE);
        carry = q;
    }
    return carry;
}

// ---------------------------------------------------------------- 测试数据
// 模拟真实卷积系数: 三角形包络 * 随机, 峰值贴近 float_len*(BASE-1)^2
static void gen(std::vector<double> &v, size_t n, uint64_t seed, double peak)
{
    std::mt19937_64 rng(seed);
    for (size_t i = 0; i < n; i++)
    {
        double env = double(std::min(i + 1, n - i)) / double(n / 2);   // 三角包络
        double r = double(rng() >> 11) / double(1ULL << 53);
        double val = std::floor(peak * env * r);
        // 加 FFT 风格的小误差
        double e = (double(rng() >> 11) / double(1ULL << 53) - 0.5) * 0.4;
        v[i] = val + e;
    }
}

// 对抗性数据: 大量 9999 制造长进位传播
static void gen_adv(std::vector<double> &v, size_t n, uint64_t seed)
{
    std::mt19937_64 rng(seed);
    for (size_t i = 0; i < n; i++)
    {
        uint32_t r = uint32_t(rng() % 100);
        double val;
        if (r < 60)      val = 9999.0;
        else if (r < 70) val = 0.0;
        else if (r < 80) val = 10000.0;
        else if (r < 90) val = 19999.0;
        else             val = double(rng() % 20000000000000ULL);
        v[i] = val;
    }
}

int main(int argc, char **argv)
{
    size_t n = (argc > 1) ? strtoull(argv[1], nullptr, 10) : 196608;
    int rounds = (argc > 2) ? atoi(argv[2]) : 15;

    std::vector<double> v(n);
    std::vector<Limb> o1(n + 8), o2(n + 8);

    // ---------- 正确性 ----------
    int bad = 0;
    for (int t = 0; t < 40; t++)
    {
        size_t nn = (t < 20) ? n : (64 + (size_t)t * 37);
        if (nn > n) nn = n;
        std::vector<double> vv(nn);
        if (t % 2 == 0) gen(vv, nn, 1000 + t, 1.9e13);
        else            gen_adv(vv, nn, 2000 + t);
        std::vector<Limb> a(nn + 8, 0), b(nn + 8, 0);
        uint64_t ca = carry_scalar(vv.data(), nn, a.data());
        uint64_t cb = carry_simd(vv.data(), nn, b.data());
        if (ca != cb || std::memcmp(a.data(), b.data(), nn * sizeof(Limb)) != 0)
        {
            bad++;
            if (bad <= 3)
            {
                size_t j = 0;
                while (j < nn && a[j] == b[j]) j++;
                printf("MISMATCH t=%d n=%zu carry %llu vs %llu  first_diff@%zu %u vs %u\n",
                       t, nn, (unsigned long long)ca, (unsigned long long)cb, j,
                       j < nn ? a[j] : 0, j < nn ? b[j] : 0);
            }
        }
    }
    printf("correctness: %s (%d/40 bad)\n", bad ? "FAIL" : "OK", bad);
    if (bad) return 1;

    // ---------- 计时 (交替 + median of ratios) ----------
    gen(v, n, 42, 1.9e13);
    // 预热
    for (int i = 0; i < 3; i++) { carry_scalar(v.data(), n, o1.data()); carry_simd(v.data(), n, o2.data()); }

    std::vector<double> ts, tv, ratio;
    for (int r = 0; r < rounds; r++)
    {
        auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 8; k++) carry_scalar(v.data(), n, o1.data());
        auto t1 = std::chrono::steady_clock::now();
        for (int k = 0; k < 8; k++) carry_simd(v.data(), n, o2.data());
        auto t2 = std::chrono::steady_clock::now();
        double a = std::chrono::duration<double>(t1 - t0).count();
        double b = std::chrono::duration<double>(t2 - t1).count();
        ts.push_back(a); tv.push_back(b); ratio.push_back(b / a);
    }
    std::sort(ts.begin(), ts.end());
    std::sort(tv.begin(), tv.end());
    std::sort(ratio.begin(), ratio.end());
    double ms = ts[rounds / 2] / 8 * 1e9 / double(n);
    double mv = tv[rounds / 2] / 8 * 1e9 / double(n);
    printf("n=%zu  scalar %.3f ns/limb   simd %.3f ns/limb   ratio(median) %.4f   speedup %.2fx\n",
           n, ms, mv, ratio[rounds / 2], 1.0 / ratio[rounds / 2]);
    // 防优化
    volatile Limb sink = Limb(o1[n / 2] + o2[n / 2]);
    (void)sink;
    return 0;
}
