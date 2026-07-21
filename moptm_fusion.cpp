// AZZRCN
// https://github.com/AZZRCN
//
// 三合一提交文件: 提交 LC 时取消注释对应 #define 即可切换 ADD / MUL / DIV
//   #define HINT_OP_ADD    // Addition of Big Integers
//   #define HINT_OP_MUL    // Multiplication of Big Integers
#define HINT_OP_DIV          // Division of Big Integers (默认)

// 禁用 cyclic (2NXN mod B^m-1) 路径: LC 实测 burnikel_ziegler_bound / r_nearly_zero 等用例
// 触发 assert 和 WA (商差1), 根因是 cyclic unwrap 精度问题. 禁用后回退到线性卷积 + absInvNewton,
// 保证正确性. 性能损失 ~7% (1M/500k 18.3ms → ~19.7ms)
#define DISABLE_2NXN_CYCLIC

#ifndef HINT_MINI_HPP
#define HINT_MINI_HPP

#include <iostream>
#include <vector>
#include <string>
#include <complex>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <algorithm>
#include <immintrin.h>  // AVX2/SSE2 SIMD (emmintrin.h 子集, 含 AVX2 intrinsics)

#include <chrono>
#ifdef PROFILE_DIV
#include <cstdio>
static FILE *_prof_fp = nullptr;
static inline void _prof_init() { if (!_prof_fp) _prof_fp = std::fopen("prof_detail.log", "w"); }
static inline void _prof_flush() { if (_prof_fp) { std::fflush(_prof_fp); } }
#define PROF_PRINT(...) do { _prof_init(); if (_prof_fp) std::fprintf(_prof_fp, __VA_ARGS__); } while(0)
#else
#define PROF_PRINT(...) ((void)0)
#endif

namespace hint
{
    using Float32 = float;
    using Float64 = double;
    using Complex32 = std::complex<Float32>;
    using Complex64 = std::complex<Float64>;

    constexpr Float64 HINT_PI = 3.141592653589793238462643;
    constexpr Float64 HINT_2PI = HINT_PI * 2;
    constexpr Float64 COS_PI_8 = 0.707106781186547524400844;

    constexpr size_t FFT_MAX_LEN = size_t(1) << 23;

    template <typename T>
    constexpr T int_floor2(T n)
    {
        constexpr int bits = sizeof(n) * 8;
        for (int i = 1; i < bits; i *= 2)
        {
            n |= (n >> i);
        }
        return (n >> 1) + 1;
    }

    template <typename T>
    constexpr T int_ceil2(T n)
    {
        constexpr int bits = sizeof(n) * 8;
        n--;
        for (int i = 1; i < bits; i *= 2)
        {
            n |= (n >> i);
        }
        return n + 1;
    }

    template <typename IntTy>
    constexpr bool is_2pow(IntTy n)
    {
        return n != 0 && (n & (n - 1)) == 0;
    }

    
    template <typename T>
    constexpr int hint_log2(T n)
    {
        constexpr int bits = sizeof(n) * 8;
        int l = -1, r = bits;
        while ((l + 1) != r)
        {
            int mid = (l + r) / 2;
            if ((T(1) << mid) > n)
            {
                r = mid;
            }
            else
            {
                l = mid;
            }
        }
        return l;
    }
    constexpr int hint_ctz(uint32_t x)
    {
        int r0 = 31;
        x &= (0 - x);
        if (x & 0x55555555)
        {
            r0 &= ~1;
        }
        if (x & 0x33333333)
        {
            r0 &= ~2;
        }
        if (x & 0x0F0F0F0F)
        {
            r0 &= ~4;
        }
        if (x & 0x00FF00FF)
        {
            r0 &= ~8;
        }
        if (x & 0x0000FFFF)
        {
            r0 &= ~16;
        }
        r0 += (x == 0);
        return r0;
    }

    constexpr int hint_ctz(uint64_t x)
    {
        int r0 = 63;
        x &= (0 - x);
        if (x & 0x5555555555555555)
        {
            r0 &= ~1; 
        }
        if (x & 0x3333333333333333)
        {
            r0 &= ~2; 
        }
        if (x & 0x0F0F0F0F0F0F0F0F)
        {
            r0 &= ~4; 
        }
        if (x & 0x00FF00FF00FF00FF)
        {
            r0 &= ~8; 
        }
        if (x & 0x0000FFFF0000FFFF)
        {
            r0 &= ~16; 
        }
        if (x & 0x00000000FFFFFFFF)
        {
            r0 &= ~32; 
        }
        r0 += (x == 0);
        return r0;
    }

    
    constexpr int hint_clz(uint32_t x)
    {
        constexpr uint32_t MASK32 = uint32_t(0xFFFF) << 16;
        int res = sizeof(uint32_t) * CHAR_BIT;
        if (x & MASK32)
        {
            res -= 16;
            x >>= 16;
        }
        if (x & (MASK32 >> 8))
        {
            res -= 8;
            x >>= 8;
        }
        if (x & (MASK32 >> 12))
        {
            res -= 4;
            x >>= 4;
        }
        if (x & (MASK32 >> 14))
        {
            res -= 2;
            x >>= 2;
        }
        if (x & (MASK32 >> 15))
        {
            res -= 1;
            x >>= 1;
        }
        return res - x;
    }
    
    constexpr int hint_clz(uint64_t x)
    {
        if (x & (uint64_t(0xFFFFFFFF) << 32))
        {
            return hint_clz(uint32_t(x >> 32));
        }
        return hint_clz(uint32_t(x)) + 32;
    }

    
    template <typename IntTy>
    constexpr int hint_bit_length(IntTy x)
    {
        if (0 == x)
        {
            return 0;
        }
        return sizeof(IntTy) * CHAR_BIT - hint_clz(x);
    }

    
    template <typename T, typename T1>
    constexpr T qpow(T m, T1 n)
    {
        T result = 1;
        while (true)
        {
            if (n & 1)
            {
                result *= m;
            }
            if (0 == n)
            {
                break;
            }
            m *= m;
            n >>= 1;
        }
        return result;
    }

    constexpr int hint_popcnt(uint32_t n)
    {
        constexpr uint32_t mask55 = 0x55555555;
        constexpr uint32_t mask33 = 0x33333333;
        constexpr uint32_t mask0f = 0x0f0f0f0f;
        constexpr uint32_t maskff = 0x00ff00ff;
        n = (n & mask55) + ((n >> 1) & mask55);
        n = (n & mask33) + ((n >> 2) & mask33);
        n = (n & mask0f) + ((n >> 4) & mask0f);
        n = (n & maskff) + ((n >> 8) & maskff);
        return uint16_t(n) + (n >> 16);
    }
    constexpr int hint_popcnt(uint64_t n)
    {
        constexpr uint64_t mask5555 = 0x5555555555555555;
        constexpr uint64_t mask3333 = 0x3333333333333333;
        constexpr uint64_t mask0f0f = 0x0f0f0f0f0f0f0f0f;
        constexpr uint64_t mask00ff = 0x00ff00ff00ff00ff;
        constexpr uint64_t maskffff = 0x0000ffff0000ffff;
        n = (n & mask5555) + ((n >> 1) & mask5555);
        n = (n & mask3333) + ((n >> 2) & mask3333);
        n = (n & mask0f0f) + ((n >> 4) & mask0f0f);
        n = (n & mask00ff) + ((n >> 8) & mask00ff);
        n = (n & maskffff) + ((n >> 16) & maskffff);
        return uint32_t(n) + (n >> 32);
    }

    constexpr uint32_t bitrev32(uint32_t n)
    {
        constexpr uint32_t mask55 = 0x55555555;
        constexpr uint32_t mask33 = 0x33333333;
        constexpr uint32_t mask0f = 0x0f0f0f0f;
        constexpr uint32_t maskff = 0x00ff00ff;
        n = ((n & mask55) << 1) | ((n >> 1) & mask55);
        n = ((n & mask33) << 2) | ((n >> 2) & mask33);
        n = ((n & mask0f) << 4) | ((n >> 4) & mask0f);
        n = ((n & maskff) << 8) | ((n >> 8) & maskff);
        return (n << 16) | (n >> 16);
    }
    constexpr uint32_t bitrev(uint32_t n, int len)
    {
        assert(len <= 32);
        return bitrev32(n) >> (32 - len);
    }

    template <typename T>
    void fill_zero(T begin, T end)
    {
        std::memset(&begin[0], 0, (end - begin) * sizeof(T));
    }

    template <typename Float>
    struct Float2
    {
        Float x0, x1;
        using F2 = Float2;
        Float2() = default;
        constexpr Float2(Float x0, Float x1) : x0(x0), x1(x1) {}

        constexpr F2 &operator+=(const F2 &rhs)
        {
            x0 += rhs.x0;
            x1 += rhs.x1;
            return *this;
        }
        constexpr F2 &operator-=(const F2 &rhs)
        {
            x0 -= rhs.x0;
            x1 -= rhs.x1;
            return *this;
        }
        constexpr F2 &operator*=(const F2 &rhs)
        {
            x0 *= rhs.x0;
            x1 *= rhs.x1;
            return *this;
        }
        friend constexpr F2 operator+(const F2 &lhs, const F2 &rhs)
        {
            return F2(lhs.x0 + rhs.x0, lhs.x1 + rhs.x1);
        }
        friend constexpr F2 operator-(const F2 &lhs, const F2 &rhs)
        {
            return F2(lhs.x0 - rhs.x0, lhs.x1 - rhs.x1);
        }
        friend constexpr F2 operator*(const F2 &lhs, const F2 &rhs)
        {
            return F2(lhs.x0 * rhs.x0, lhs.x1 * rhs.x1);
        }
        friend constexpr F2 operator*(const F2 &lhs, const Float &rhs)
        {
            return F2(lhs.x0 * rhs, lhs.x1 * rhs);
        }
        constexpr F2 reverse() const
        {
            return F2(x1, x0);
        }
        constexpr void set1(Float x)
        {
            x0 = x1 = x;
        }
        static constexpr F2 from1(Float x)
        {
            return F2(x, x);
        }
        static constexpr F2 fromMem(const Float *p)
        {
            return F2(p[0], p[1]);
        }
        void store(Float *p) const
        {
            p[0] = x0;
            p[1] = x1;
        }
    };

    template <typename Float>
    struct Complex2
    {
        using F2 = Float2<Float>;
        using C2 = Complex2;
        F2 real, imag;
        Complex2() = default;
        constexpr Complex2(F2 r, F2 i) : real(r), imag(i) {}
        constexpr Complex2(Float r, Float i) : real(F2::from1(r)), imag(F2::from1(i)) {}
        constexpr Complex2(Float x0, Float x1, Float x2, Float x3) : real(x0, x1), imag(x2, x3) {}

        constexpr C2 &operator+=(const C2 &rhs)
        {
            real += rhs.real;
            imag += rhs.imag;
            return *this;
        }
        constexpr C2 &operator-=(const C2 &rhs)
        {
            real -= rhs.real;
            imag -= rhs.imag;
            return *this;
        }
        constexpr C2 &operator*=(const C2 &rhs)
        {
            F2 r = real * rhs.real - imag * rhs.imag;
            F2 i = real * rhs.imag + imag * rhs.real;
            return *this = C2(r, i);
        }
        friend constexpr C2 operator+(const C2 &lhs, const C2 &rhs)
        {
            return C2(lhs.real + rhs.real, lhs.imag + rhs.imag);
        }
        friend constexpr C2 operator-(const C2 &lhs, const C2 &rhs)
        {
            return C2(lhs.real - rhs.real, lhs.imag - rhs.imag);
        }
        friend constexpr C2 operator*(const C2 &lhs, const F2 &rhs)
        {
            return C2(lhs.real * rhs, lhs.imag * rhs);
        }
        friend constexpr C2 operator*(const C2 &lhs, const Float &rhs)
        {
            return C2(lhs.real * rhs, lhs.imag * rhs);
        }
        constexpr C2 mul(const C2 &other) const
        {
            const F2 ii = imag * other.imag;
            const F2 ri = real * other.imag;
            const F2 r = real * other.real - ii;
            const F2 i = imag * other.real + ri;
            return C2(r, i);
        }
        constexpr C2 mulConj(const C2 &other) const
        {
            const F2 ii = imag * other.imag;
            const F2 ri = real * other.imag;
            const F2 r = real * other.real + ii;
            const F2 i = imag * other.real - ri;
            return C2(r, i);
        }
        constexpr C2 reverse() const
        {
            return C2(real.reverse(), imag.reverse());
        }
        constexpr void permute()
        {
            std::swap(real.x1, imag.x0);
        }
        void load(const Float *p)
        {
            real = F2::fromMem(p);
            imag = F2::fromMem(p + 2);
        }
        void store(Float *p) const
        {
            real.store(p);
            imag.store(p + 2);
        }
        void print() const
        {
            std::cout << '(' << real.x0 << ',' << imag.x0 << ") "
                      << '(' << real.x1 << ',' << imag.x1 << ")\n";
        }
    };

    
    namespace transform
    {

        template <typename T>
        inline void transform2(T &sum, T &diff)
        {
            T temp0 = sum, temp1 = diff;
            sum = temp0 + temp1;
            diff = temp0 - temp1;
        }

        template <typename T>
        inline void transform2(const T a, const T b, T &sum, T &diff)
        {
            sum = a + b;
            diff = a - b;
        }
        namespace fft
        {
            constexpr size_t FFT_MAX_LEN = size_t(1) << 23;

            template <typename Float>
            inline std::complex<Float> getOmega(size_t n, size_t index, Float factor = 1)
            {
                Float theta = -HINT_2PI * index / n;
                return std::polar<Float>(1, theta * factor);
            }
            template <typename Float>
            inline void difSplit(Float &r0, Float &i0, Float &r1, Float &i1, Float &r2, Float &i2, Float &r3, Float &i3)
            {
                transform2(r0, r2);
                transform2(i0, i2);
                transform2(r1, r3);
                transform2(i1, i3);

                transform2(r2, i3);
                transform2(i2, r3, r3, i2);
                std::swap(i3, r3);
            }
            template <typename Float>
            inline void iditSplit(Float &r0, Float &i0, Float &r1, Float &i1, Float &r2, Float &i2, Float &r3, Float &i3)
            {
                transform2(r2, r3);
                transform2(i2, i3);

                transform2(r0, r2);
                transform2(i0, i2);
                transform2(r1, i3, i3, r1);
                transform2(i1, r3);
                std::swap(i3, r3);
            }
            template <typename Float, int DIV>
            struct FFTTable
            {
                using C2 = Complex2<Float>;
                FFTTable(int factor_in) : factor(factor_in), table(8)
                {
                    size_t len = table.size(), rank = len * DIV / 4;
                    auto it = getBegin(rank);
                    Float theta = -HINT_2PI * factor / rank;
                    table[4] = 1, table[6] = 0;
                    table[5] = std::cos(theta), table[7] = std::sin(theta);
                }
                void expandLog(int log_len)
                {
                    expand(size_t(1) << log_len);
                }
                void expand(size_t fft_len)
                {
                    size_t cur_len = table.size() * DIV / 4;
                    if (fft_len <= cur_len)
                    {
                        return;
                    }
                    size_t new_len = fft_len * 4 / DIV;
                    table.resize(new_len);
                    for (size_t rank = cur_len * 2; rank <= fft_len; rank *= 2)
                    {
                        auto it = getBegin(rank), last_it = getBegin(rank / 2);
                        Float theta = -HINT_2PI * factor / rank;
                        C2 unit(std::cos(theta), std::sin(theta));
                        size_t len = rank * 2 / DIV;
                        for (auto end = it + len; it < end; it += 8, last_it += 4)
                        {
                            C2 omega0, omega1;
                            omega0.load(last_it);
                            omega1 = omega0.mul(unit);
                            std::swap(omega0.real.x1, omega1.real.x0);
                            std::swap(omega0.imag.x1, omega1.imag.x0);
                            omega0.store(it);
                            omega1.store(it + 4);
                        }
                    }
                }
                constexpr const Float *getBegin(size_t rank) const
                {
                    return &table[rank * 2 / DIV];
                }
                constexpr Float *getBegin(size_t rank)
                {
                    return &table[rank * 2 / DIV];
                }
                std::vector<Float> table;
                int factor;
            };

            template <typename Float>
            class FFT
            {
                using Table = FFTTable<Float, 4>;
                using F2 = Float2<Float>;
                using C2 = Complex2<Float>;

            public:
                FFT() : table1(1), table3(3) {}
                void expand(size_t float_len)
                {
                    table1.expand(float_len / 2);
                    table3.expand(float_len / 2);
                }
                template <bool RIRI_IN>
                void dif(Float inout[], size_t float_len)
                {
                    if (float_len <= 8)
                    {
                        difSmall<RIRI_IN>(inout, float_len);
                        return;
                    }
                    expand(float_len);
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    auto tp1 = reinterpret_cast<const C2 *>(table1.getBegin(fft_len));
                    auto tp3 = reinterpret_cast<const C2 *>(table3.getBegin(fft_len));
                    auto it = reinterpret_cast<C2 *>(inout);
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        C2 c0 = it[0], c1 = it[stride1], c2 = it[stride2], c3 = it[stride3];
                        if (RIRI_IN)
                        {
                            c0.permute(), c1.permute(), c2.permute(), c3.permute();
                        }
                        difSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                        it[0] = c0, it[stride1] = c1, it[stride2] = c2.mul(tp1[0]), it[stride3] = c3.mul(tp3[0]);
                    }
                    size_t stride = float_len / 4;
                    dif<false>(inout, stride * 2);
                    dif<false>(inout + stride * 2, stride);
                    dif<false>(inout + stride * 3, stride);
                }
                template <bool RIRI_OUT>
                void idit(Float inout[], size_t float_len)
                {
                    if (float_len <= 8)
                    {
                        iditSmall<RIRI_OUT>(inout, float_len);
                        return;
                    }
                    expand(float_len);
                    size_t stride = float_len / 4;
                    idit<false>(inout, stride * 2);
                    idit<false>(inout + stride * 2, stride);
                    idit<false>(inout + stride * 3, stride);
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    auto tp1 = reinterpret_cast<const C2 *>(table1.getBegin(fft_len));
                    auto tp3 = reinterpret_cast<const C2 *>(table3.getBegin(fft_len));
                    auto it = reinterpret_cast<C2 *>(inout);
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        C2 c0 = it[0], c1 = it[stride1], c2 = it[stride2].mulConj(tp1[0]), c3 = it[stride3].mulConj(tp3[0]);
                        iditSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                        if (RIRI_OUT)
                        {
                            c0.permute(), c1.permute(), c2.permute(), c3.permute();
                        }
                        it[0] = c0, it[stride1] = c1, it[stride2] = c2, it[stride3] = c3;
                    }
                }

                template <bool RIRI_IN>
                void difSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2)
                    {
                        return;
                    }
                    auto itc = reinterpret_cast<C2 *>(inout);
                    auto itf = reinterpret_cast<F2 *>(inout);
                    if (float_len == 4) 
                    {
                        if (RIRI_IN)
                        {
                            std::swap(inout[1], inout[2]);
                        }
                        transform2(inout[0], inout[1]);
                        transform2(inout[2], inout[3]);
                    }
                    else 
                    {
                        if (RIRI_IN)
                        {
                            std::swap(inout[1], inout[2]);
                            std::swap(inout[5], inout[6]);
                        }
                        Float r0 = inout[0], r1 = inout[1], i0 = inout[2], i1 = inout[3];
                        Float r2 = inout[4], r3 = inout[5], i2 = inout[6], i3 = inout[7];
                        difSplit(r0, i0, r1, i1, r2, i2, r3, i3);
                        transform2(r0, r1);
                        transform2(i0, i1);
                        inout[0] = r0, inout[1] = r1, inout[2] = i0, inout[3] = i1;
                        inout[4] = r2, inout[5] = r3, inout[6] = i2, inout[7] = i3;
                    }
                }
                template <bool RIRI_OUT>
                void iditSmall(Float inout[], size_t float_len)
                {
                    if (float_len <= 2)
                    {
                        return;
                    }
                    auto itc = reinterpret_cast<C2 *>(inout);
                    if (float_len == 4) 
                    {
                        transform2(inout[0], inout[1]);
                        transform2(inout[2], inout[3]);
                        if (RIRI_OUT)
                        {
                            std::swap(inout[1], inout[2]);
                        }
                    }
                    else 
                    {
                        Float r0 = inout[0], r1 = inout[1], i0 = inout[2], i1 = inout[3];
                        Float r2 = inout[4], r3 = inout[5], i2 = inout[6], i3 = inout[7];
                        transform2(r0, r1);
                        transform2(i0, i1);
                        iditSplit(r0, i0, r1, i1, r2, i2, r3, i3);
                        inout[0] = r0, inout[1] = r1, inout[2] = i0, inout[3] = i1;
                        inout[4] = r2, inout[5] = r3, inout[6] = i2, inout[7] = i3;
                        if (RIRI_OUT)
                        {
                            std::swap(inout[1], inout[2]);
                            std::swap(inout[5], inout[6]);
                        }
                    }
                }

            private:
                Table table1, table3;
            };

            template <typename Float>
            class BinRevTableC2HP
            {
            public:
                using C1 = std::complex<Float>;
                using C2 = Complex2<Float>;
                static constexpr int MAX_LOG_LEN = 32, LOG_BLOCK = 1, BLOCK = 1 << LOG_BLOCK;
                static constexpr size_t MAX_LEN = size_t(1) << MAX_LOG_LEN;

                BinRevTableC2HP(int log_max_iter_in, int log_fft_len_in)
                    : index(0), pop(0), log_max_iter(log_max_iter_in), log_fft_len(log_fft_len_in)
                {
                    assert(log_max_iter <= log_fft_len);
                    assert(log_fft_len <= MAX_LOG_LEN);
                    const Float factor = Float(1) / (size_t(1) << (log_fft_len - log_max_iter));
                    for (int i = 0; i < MAX_LOG_LEN; i++)
                    {
                        units[i] = getOmega(size_t(1) << (i + 1), 1, factor);
                    }
                    auto fp = reinterpret_cast<Float *>(table);
                    fp[0] = 1, fp[BLOCK] = 0;
                    for (int i = 1; i < BLOCK; i++)
                    {
                        C1 omega = getOmega(BLOCK, bitrev(i, LOG_BLOCK), factor);
                        fp[i] = omega.real(), fp[i + BLOCK] = omega.imag();
                    }
         }
         
                void reset(size_t i = 0)
                {
                    if (i == 0)
                    {
                        pop = 0, index = i;
                        return;
                    }
                    assert((i & (i - 1)) == 0);
                    assert(i % BLOCK == 0);
                    pop = 1, index = i / BLOCK;
                    int zero = hint_ctz(index);
                    auto fp = reinterpret_cast<Float *>(&units[zero + 1]);
                    table[1].real.set1(fp[0]);
                    table[1].imag.set1(fp[1]);
                    table[1] = table[1].mul(table[0]);
                }
                C2 iterate()
                {
                    C2 res = table[pop], unitx;
                    index++;
                    int zero = hint_ctz(index);
                    auto fp = reinterpret_cast<Float *>(&units[zero + 1]);
                    unitx.real.set1(fp[0]);
                    unitx.imag.set1(fp[1]);
                    pop -= zero;
                    table[pop + 1] = table[pop].mul(unitx);
                    pop++;
                    return res;
                }

            private:
                C1 units[MAX_LOG_LEN]{};
                C2 table[MAX_LOG_LEN]{};
                size_t index;
                int pop;
                int log_max_iter, log_fft_len;
            };

            template <size_t RI_DIFF = 1, typename Float>
            inline void dot_rfft(Float *inout0, Float *inout1, const Float *in0, const Float *in1,
                                 const std::complex<Float> &omega0, const Float factor = 1)
            {
                using Complex = std::complex<Float>;
                auto mul1 = [](Complex c0, Complex c1)
                {
                    return Complex(c0.imag() * c1.real() + c0.real() * c1.imag(),
                                   c0.imag() * c1.imag() - c0.real() * c1.real());
                };
                auto mul2 = [](Complex c0, Complex c1)
                {
                    return Complex(c0.real() * c1.imag() - c0.imag() * c1.real(),
                                   c0.real() * c1.real() + c0.imag() * c1.imag());
                };
                auto compute2 = [&omega0](Complex in0, Complex in1, Complex &out0, Complex &out1, auto Func)
                {
                    in1 = std::conj(in1);
                    transform2(in0, in1);
                    in1 = Func(in1, omega0);
                    out0 = in0 + in1;
                    out1 = std::conj(in0 - in1);
                };
                Complex c0, c1;
                {
                    Complex x0, x1, x2, x3;
                    c0.real(inout0[0]), c0.imag(inout0[RI_DIFF]), c1.real(inout1[0]), c1.imag(inout1[RI_DIFF]);
                    compute2(c0, c1, x0, x1, mul1);
                    c0.real(in0[0]), c0.imag(in0[RI_DIFF]), c1.real(in1[0]), c1.imag(in1[RI_DIFF]);
                    compute2(c0, c1, x2, x3, mul1);
                    x0 *= x2 * factor;
                    x1 *= x3 * factor;
                    compute2(x0, x1, c0, c1, mul2);
                }
                inout0[0] = c0.real(), inout0[RI_DIFF] = c0.imag();
                inout1[0] = c1.real(), inout1[RI_DIFF] = c1.imag();
            }
            template <typename Float>
            inline void dot_rfftX2(Float *inout0, Float *inout1, const Float *in0, const Float *in1, const Complex2<Float> &omega0, const Float2<Float> &inv)
            {
                using C2 = Complex2<Float>;
                auto mul1 = [](C2 c0, C2 c1)
                {
                    return C2(c0.imag * c1.real + c0.real * c1.imag,
                              c0.imag * c1.imag - c0.real * c1.real);
                };
                auto mul2 = [](C2 c0, C2 c1)
                {
                    return C2(c0.real * c1.imag - c0.imag * c1.real,
                              c0.real * c1.real + c0.imag * c1.imag);
                };
                auto compute2 = [&omega0](C2 c0, C2 c1, C2 &out0, C2 &out1, auto Func)
                {
                    C2 t0(c0.real + c1.real, c0.imag - c1.imag), t1(c0.real - c1.real, c0.imag + c1.imag);
                    t1 = Func(t1, omega0);
                    out0 = t0 + t1;
                    out1.real = t0.real - t1.real;
                    out1.imag = t1.imag - t0.imag;
                };
                C2 c0, c1;
                {
                    C2 x0, x1, x2, x3;
                    c0.load(inout0), c1.load(inout1);
                    compute2(c0, c1.reverse(), x0, x1, mul1);

                    c0.load(in0), c1.load(in1);
                    compute2(c0, c1.reverse(), x2, x3, mul1);
                    c0 = x0.mul(x2) * inv;
                    c1 = x1.mul(x3) * inv;
                    compute2(c0, c1, c0, c1, mul2);
                }
                c0.store(inout0), c1.reverse().store(inout1);
            }
            
            template <size_t RI_DIFF = 1, typename Float>
            inline void real_dot_binrev(Float in_out[], const Float in[], size_t float_len, Float inv = -1)
            {
                constexpr size_t MAX_LEN = 32;
                constexpr int LOG_LEN = hint_log2(MAX_LEN);
                static_assert(is_2pow(RI_DIFF));
                static_assert(RI_DIFF <= 8);
                assert(is_2pow(float_len));
                assert(float_len <= MAX_LEN);
                if (float_len < 2)
                {
                    return;
                }
                assert(float_len >= RI_DIFF * 2);
                auto idx_trans = [](size_t idx)
                {
                    return (idx / RI_DIFF) * RI_DIFF * 2 + idx % RI_DIFF;
                };
                auto get_omega = [](size_t idx, size_t rank)
                {
                    return std::polar<Float>(1, -HINT_PI * Float(idx) / rank);
                };
                using Complex = std::complex<Float>;
                static const Complex table[]{
                    get_omega(bitrev(4, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(5, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(8, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(9, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(10, LOG_LEN), MAX_LEN),
                    get_omega(bitrev(11, LOG_LEN), MAX_LEN),
                };
                inv = inv < 0 ? Float(2) / float_len : inv * Float(2);
                auto r0 = in_out[0], i0 = in_out[RI_DIFF], r1 = in[0], i1 = in[RI_DIFF];
                transform2(r0, i0);
                transform2(r1, i1);
                r0 *= r1, i0 *= i1;
                transform2(r0, i0);
                in_out[0] = r0 * 0.5 * inv, in_out[RI_DIFF] = i0 * 0.5 * inv;
                if (float_len >= 4)
                {
                    Complex temp(in_out[idx_trans(1)], in_out[idx_trans(1) + RI_DIFF]);
                    temp *= Complex(in[idx_trans(1)], in[idx_trans(1) + RI_DIFF]) * inv;
                    in_out[idx_trans(1)] = temp.real(), in_out[idx_trans(1) + RI_DIFF] = temp.imag();
                }
                if (float_len >= 8)
                {
                    inv *= Float(0.125);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(2)], &in_out[idx_trans(3)],
                                      &in[idx_trans(2)], &in[idx_trans(3)], Complex(COS_PI_8, -COS_PI_8), inv);
                }

                if (float_len >= 16)
                {
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(4)], &in_out[idx_trans(7)],
                                      &in[idx_trans(4)], &in[idx_trans(7)], table[0], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(5)], &in_out[idx_trans(6)],
                                      &in[idx_trans(5)], &in[idx_trans(6)], table[1], inv);
                }
                if (float_len >= 32)
                {
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(8)], &in_out[idx_trans(15)],
                                      &in[idx_trans(8)], &in[idx_trans(15)], table[2], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(9)], &in_out[idx_trans(14)],
                                      &in[idx_trans(9)], &in[idx_trans(14)], table[3], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(10)], &in_out[idx_trans(13)],
                                      &in[idx_trans(10)], &in[idx_trans(13)], table[4], inv);
                    dot_rfft<RI_DIFF>(&in_out[idx_trans(11)], &in_out[idx_trans(12)],
                                      &in[idx_trans(11)], &in[idx_trans(12)], table[5], inv);
                }
            }

            template <typename Float>
            inline void real_dot_binrev2(Float in_out[], const Float in[], size_t float_len)
            {
                using F2 = Float2<Float>;
                Float inv = 1.0 / float_len;
                real_dot_binrev<2>(in_out, in, 16, inv);
                inv = 0.25 / float_len;
                const F2 invx = F2::from1(inv);
                BinRevTableC2HP<Float> table(31, 32);
                for (size_t begin = 16; begin < float_len; begin *= 2)
                {
                    table.reset(begin / 2);
                    auto it0 = in_out + begin, it1 = it0 + begin - 4;
                    auto it2 = in + begin, it3 = it2 + begin - 4;
                    for (; it0 < it1; it0 += 4, it1 -= 4, it2 += 4, it3 -= 4)
                    {
                        dot_rfftX2(it0, it1, it2, it3, table.iterate(), invx);
                    }
                }
            }

            // 共享 FFT 对象: 所有函数复用同一份旋转因子表，避免首次调用时重复计算
            template <typename Float>
            inline FFT<Float> &getSharedFFT()
            {
                static FFT<Float> fft;
                return fft;
            }

            template <typename Float>
            inline void real_conv(Float *in_out1, Float *in2, size_t float_len)
            {
                assert(is_2pow(float_len));
                assert(float_len <= FFT_MAX_LEN * 2);
                auto &fft = getSharedFFT<Float>();
                fft.expand(float_len);
                fft.template dif<true>(in_out1, float_len);
                if (in_out1 != in2)
                {
                    fft.template dif<true>(in2, float_len);
                }
                real_dot_binrev2(in_out1, in2, float_len);
                fft.template idit<true>(in_out1, float_len);
            }
        }
    }
    constexpr size_t count_base10(uint64_t num)
    {
        size_t count = 0;
        while (num)
        {
            num /= 10;
            count++;
        }
        return count;
    }
    // 64KB 查表法：2 字节 ASCII → 0-99 (从 fusion.cpp 移植)
    struct ParseTable {
        uint8_t table[0x10000];
        constexpr ParseTable() : table() {
            for (uint32_t i = 48; i < 58; ++i)
                for (uint32_t j = 48; j < 58; ++j)
                    table[i << 8 | j] = uint8_t((i & 15) * 10 + (j & 15));
        }
        inline uint32_t operator()(const char* s) const {
            return table[uint32_t(uint8_t(s[0])) << 8 | uint32_t(uint8_t(s[1]))];
        }
    };
    static constexpr ParseTable parseTable{};

    inline uint16_t str4toi(const char *s)
    {
        return uint16_t(parseTable(s) * 100 + parseTable(s + 2));
    }
    // 16 字节 ASCII 数字 → 4 个 uint16 limbs (纯 SSE2, x86-64 baseline)
    // 倒序解析: bytes[0..3]   -> limb[3] (最高位)
    //          bytes[4..7]   -> limb[2]
    //          bytes[8..11]  -> limb[1]
    //          bytes[12..15] -> limb[0] (最低位)
    // 注意: 不用 _mm_maddubs_epi16 (那是 SSSE3, 编译命令无 -mssse3 会报错),
    //       改用 SSE2 的 unpacklo/hi + mullo_epi16 + madd_epi16(ones) 等价实现。
    //       另修正任务原稿 mul4 常量布局 bug: 必须用 _mm_set1_epi32(0x00010064)
    //       (low word=100, high word=1), 否则奇数 word=0 → 12*100+34*0=1200。
    inline void str16to4limbs(const char *p, uint16_t *limbs4)
    {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        v = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i zero = _mm_setzero_si128();
        // 拆成低/高 8 字节, 各零扩展到 8 个 16 位
        __m128i lo = _mm_unpacklo_epi8(v, zero);  // [b0..b7] as words
        __m128i hi = _mm_unpackhi_epi8(v, zero);  // [b8..b15] as words
        // 字节对 (10,1) → 2 位数: 先按 [10,1,10,1,...] 缩放, 再用 madd(ones) 合并相邻
        __m128i mul2 = _mm_set_epi16(1, 10, 1, 10, 1, 10, 1, 10);  // low→high: [10,1,...]
        __m128i lo_s = _mm_mullo_epi16(lo, mul2);
        __m128i hi_s = _mm_mullo_epi16(hi, mul2);
        __m128i ones = _mm_set1_epi16(1);
        __m128i lo_d = _mm_madd_epi16(lo_s, ones);  // 4 dwords: [b0b1, b2b3, b4b5, b6b7]
        __m128i hi_d = _mm_madd_epi16(hi_s, ones);  // 4 dwords: [b8b9, b10b11, b12b13, b14b15]
        // 打包 8 个 dword(0-99) → 8 个 word
        __m128i packed = _mm_packs_epi32(lo_d, hi_d);
        // 字对 (100,1) → 4 位数
        __m128i mul4 = _mm_set1_epi32(0x00010064);  // low word=100, high word=1
        __m128i v4 = _mm_madd_epi16(packed, mul4);  // 4 dwords: [bytes0-3, bytes4-7, bytes8-11, bytes12-15]
        // 反转 dword 顺序 → 低位优先 (与 data[0]=LSB 一致)
        v4 = _mm_shuffle_epi32(v4, _MM_SHUFFLE(0, 1, 2, 3));
        // 打包 4 dword(0-9999, 不溢出 int16) → 4 word 并存储
        __m128i result = _mm_packs_epi32(v4, v4);
        _mm_storel_epi64(reinterpret_cast<__m128i *>(limbs4), result);
    }
    constexpr void itostr4(uint16_t n, char *s)
    {
        s[0] = n / 1000 + '0';
        s[1] = n / 100 % 10 + '0';
        s[2] = n / 10 % 10 + '0';
        s[3] = n % 10 + '0';
    }
    template <typename T>
    struct ViewTy
    {
        const T *ptr;
        size_t size;
        ViewTy() = default;
        ViewTy(const T *ptr, size_t size) : ptr(ptr), size(size) {}
        const T &operator[](size_t index) const
        {
            return ptr[index];
        }
        ViewTy operator+(size_t offset) const
        {
            assert(offset <= size);
            return ViewTy{ptr + offset, size - offset};
        }
        const T *begin() const
        {
            return ptr;
        }
        const T *end() const
        {
            return ptr + size;
        }
    };
    template <typename T>
    struct SpanTy
    {
        T *ptr;
        size_t size;
        SpanTy() = default;
        SpanTy(T *ptr, size_t size) : ptr(ptr), size(size) {}
        const T &operator[](size_t index) const
        {
            return ptr[index];
        }
        T &operator[](size_t index)
        {
            return ptr[index];
        }
        operator ViewTy<T>()
        {
            return ViewTy<T>{ptr, size};
        }
        SpanTy operator+(size_t offset) const
        {
            assert(offset <= size);
            return SpanTy{ptr + offset, size - offset};
        }
        const T *begin() const
        {
            return ptr;
        }
        const T *end() const
        {
            return ptr + size;
        }
        T *begin()
        {
            return ptr;
        }
        T *end()
        {
            return ptr + size;
        }
    };

    template <typename T>
    T add_half(T a, T b, T base, T &cf)
    {
        T r = a + b;
        cf = r >= base;
        T mask = T(0) - T(cf);
        return r - (base & mask);
    }
    template <typename T>
    T sub_half(T a, T b, T base, T &bf)
    {
        bf = a < b;
        T mask = T(0) - T(bf);
        return a - b + (base & mask);
    }

    template <typename T>
    constexpr size_t count_true_length(const T array[], size_t length)
    {
        if (nullptr == array)
        {
            return 0;
        }
        while (length > 0 && array[length - 1] == 0)
        {
            length--;
        }
        return length;
    }

    // OPT-1: writeTo 输出 4 位查表移到命名空间作用域 constexpr，
    // 消除函数局部 static 的 magic static guard 运行时开销
    namespace
    {
        struct OutTable
        {
            uint32_t t[10000];
            constexpr OutTable() : t()
            {
                for (int i = 0; i < 10000; i++)
                {
                    t[i] = uint32_t(i / 1000 + '0') |
                           (uint32_t(i / 100 % 10 + '0') << 8) |
                           (uint32_t(i / 10 % 10 + '0') << 16) |
                           (uint32_t(i % 10 + '0') << 24);
                }
            }
        };
        constexpr OutTable outTable{};
    }

    class Integer
    {
    public:
        using Limb = uint16_t;
        using Limb2 = uint32_t;
        using DataVec = std::vector<Limb>;
        using Span = SpanTy<Limb>;
        using View = ViewTy<Limb>;
        static constexpr Limb BASE_DIGIT = 4;
        static constexpr Limb BASE = qpow(10, BASE_DIGIT);
        static constexpr Limb HALF_BASE = BASE / 2;
        // Barrett reduction for s/BASE and s%BASE (BASE=10000):
        //   M = ceil(2^64 / 10000) = 0x68DB8BAC710CC
        //   q = divBASE(s) = (uint64_t)((unsigned __int128)s * M >> 64) == s / 10000
        //   r = s - q * BASE                                              == s % 10000
        // S=64 means q = high 64 bits of 128-bit product (no shift needed, one `mul` insn).
        // GCC default uses S=75 (mulq + shrq $11); S=64 saves the shrq in the serial carry chain.
        // Correct for s <= 2200231879019999 (~2^51). FFT convolution sums are bounded by
        // N*(BASE-1)^2 ~ N*10^8; for double-precision FFT N < 2^50/10^8 ~ 11263 limbs
        // => s_max ~ 1.1e12, well within the safe range (2000x margin).
        static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CCULL;
        static uint64_t divBASE(uint64_t s) { return (uint64_t)((unsigned __int128)s * BARRETT_M >> 64); }
        Integer() : data(), sign(false) {}
        
        Integer(const Integer &input) = default;
        
        Integer(Integer &&input) = default;
        
        Integer(const std::string &input) : sign(false)
        {
            fromString(input);
        }
        Integer(const char *input) : sign(false)
        {
            fromString(input);
        }
        Integer(View input) : sign(false), data(input.begin(), input.end())
        {
            removeLeadingZero();
        }
        Integer(Span input) : sign(false), data(input.begin(), input.end())
        {
            removeLeadingZero();
        }
        
        template <typename T>
        Integer(const T &input)
        {
            sign = input < 0;
            uint64_t num = std::abs(input);
            while (num > 0)
            {
                data.push_back(num % BASE);
                num /= BASE;
            }
        }
        
        Integer &operator=(const Integer &input) = default;
        
        Integer &operator=(Integer &&input) = default;

        View getView() const
        {
            return View{data.data(), data.size()};
        }
        Span getSpan()
        {
            return Span{data.data(), data.size()};
        }

        bool isOdd() const
        {
            if (length() == 0)
            {
                return false;
            }
            return data[0] % 2 == 1;
        }
        bool isEven() const
        {
            return !isOdd();
        }
        bool isZero() const
        {
            return length() == 0;
        }
        
        void setSign(bool new_sign)
        {
            sign = new_sign;
        }
        
        bool isNeg() const
        {
            return sign && (length() > 0);
        }
        size_t length() const
        {
            return data.size();
        }
        size_t lengthBase10() const
        {
            size_t len = length();
            if (len == 0)
            {
                return 1;
            }
            return (len - 1) * BASE_DIGIT + count_base10(data[len - 1]);
        }
        void removeLeadingZero()
        {
            size_t len = length();
            len = count_true_length(data.data(), len);
            data.resize(len);
            sign = sign && (len > 0);
        }
        void clear()
        {
            data.clear();
            sign = false;
        }
        void fromString(const std::string &str)
        {
            fromCharRange(str.data(), str.data() + str.size());
        }
	void from_c_str(const char * str) {
            if (str[0] == '\0')
            {
                return;
            }
	    int sz = 0;
	    for (; str[sz] != '\0'; sz++);
            auto p_begin = str, p_end = p_begin + sz;
            if (str[0] == '-')
            {
                sign = true;
                p_begin++;
            }
            size_t len = p_end - p_begin;
            data.resize((len + BASE_DIGIT - 1) / BASE_DIGIT);
            size_t i = 0;
            while (p_end > p_begin + 3)
            {
                p_end -= BASE_DIGIT;
                data[i] = str4toi(p_end);
                i++;
            }
            if (p_end > p_begin)
            {
                data[i] = 0;
                while (p_end > p_begin)
                {
                    data[i] *= 10;
                    data[i] += p_begin[0] - '0';
                    p_begin++;
                }
            }
            removeLeadingZero();
		
	}
        std::string toString() const
        {
            std::string res;
            std::vector<char> buf(4);
            if (isZero())
            {
                res = "0";
            }
            else
            {
                if (isNeg())
                {
                    res = '-';
                }
                res += std::to_string(data.back());
                size_t i = data.size() - 1;
                while (i > 0)
                {
                    i--;
                    itostr4(data[i], buf.data());
                    res.append(buf.data(), 4);
                }
            }
            return res;
        }
	const char* to_c_str(char* res) const {
            
            std::vector<char> buf(4);
            if (isZero())
            {
		res[0] = '0';
		res[1] = '\0';
                
            }
            else
            {
                /*if (isNeg())
                {
                    res = '-';
                }*/
		char *p = res;
		int x = data.back();
		int cnt = 0;
		while (x) {
			cnt++;
			x /= 10;
		}
		x = data.back();
		p = res + cnt - 1;
		while (x) {
			*p = (x % 10) + '0';
			x /= 10;
			p--;
		}
		p = res + cnt;
                
                size_t i = data.size() - 1;
                while (i > 0)
                {
                    i--;
                    itostr4(data[i], buf.data());
                    
		    for (int j = 0; j < 4; j++) {
		    	*p = buf[j];
			p++;
		    }
                }
		*p = '\0';
            }
	    return res;

	}
        // 零拷贝写入到 out，返回写入字节数（不含 '\0'）
        size_t writeTo(char* out) const
        {
            // 4 位数字查表，避免 itostr4 的 4 次除法
            // OPT-1: 表已移至命名空间作用域 constexpr outTable（见 class Integer 之前），
            // 消除函数局部 static 的 magic static guard
            // 原代码（保留作注释）：
            //   static const uint32_t* table = []() {
            //       static uint32_t t[10000];
            //       for (int i = 0; i < 10000; i++) {
            //           t[i] = uint32_t(i / 1000 + '0') |
            //                  (uint32_t(i / 100 % 10 + '0') << 8) |
            //                  (uint32_t(i / 10 % 10 + '0') << 16) |
            //                  (uint32_t(i % 10 + '0') << 24);
            //       }
            //       return t;
            //   }();
            if (isZero())
            {
                out[0] = '0';
                return 1;
            }
            char* p = out;
            if (isNeg())
            {
                *p++ = '-';
            }
            // 最高位 limb 不补前导零
            uint16_t high = data.back();
            char tmp[5];
            int n = 0;
            do
            {
                tmp[n++] = char('0' + high % 10);
                high /= 10;
            } while (high);
            while (n--)
            {
                *p++ = tmp[n];
            }
            // 其余 limb 查表，每 4 位
            // OPT-2: SSE2 4× 展开，一次输出 16 字节 = 4 个 limb
            size_t i = data.size() - 1;
            while (i >= 4)
            {
                uint32_t v0 = outTable.t[data[i - 1]]; // 最高（先输出）
                uint32_t v1 = outTable.t[data[i - 2]];
                uint32_t v2 = outTable.t[data[i - 3]];
                uint32_t v3 = outTable.t[data[i - 4]]; // 最低
                // _mm_set_epi32(e3, e2, e1, e0): e3 在最高地址，e0 在最低地址
                // 输出顺序：data[i-1] -> p[0..3], data[i-2] -> p[4..7],
                //          data[i-3] -> p[8..11], data[i-4] -> p[12..15]
                __m128i v = _mm_set_epi32(v3, v2, v1, v0);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v);
                p += 16;
                i -= 4;
            }
            // 标量尾处理
            while (i > 0)
            {
                i--;
                std::memcpy(p, &outTable.t[data[i]], 4);
                p += 4;
            }
            return size_t(p - out);
        }
        // 零拷贝读取 [start, end) 的数字字符
        void fromCharRange(const char* start, const char* end)
        {
            size_t len = end - start;
            if (len == 0)
            {
                clear();
                return;
            }
            const char* p_begin = start;
            if (*p_begin == '-')
            {
                sign = true;
                p_begin++;
                len--;
            }
            else
            {
                sign = false;
            }
            data.resize((len + BASE_DIGIT - 1) / BASE_DIGIT);
            size_t i = 0;
            // 优化: 16 字节主循环 (SSE2, 一次 4 limbs) + 8/4/1-3 字节回退
            // 边界安全: 循环条件 end-p_begin>=16 保证 _mm_loadu_si128(end-16,end)
            //           读取范围 [end-16, end) 完全落在 token 内, 不会越界 mmap 末页
            while (end - p_begin >= 16)
            {
                end -= 16;
                str16to4limbs(end, data.data() + i);
                i += 4;
            }
            // 8 字节回退 (一次 2 limbs) - 保留原代码
            while (end - p_begin >= 8)
            {
                end -= 8;
                data[i] = str4toi(end + 4);     // 低 4 位
                data[i + 1] = str4toi(end);     // 高 4 位
                i += 2;
            }
            // 处理剩余 4-7 字节
            while (end - p_begin >= 4)
            {
                end -= 4;
                data[i] = str4toi(end);
                i++;
            }
            // 处理剩余 1-3 字节 (最高位)
            if (end > p_begin)
            {
                data[i] = 0;
                while (end > p_begin)
                {
                    data[i] *= 10;
                    data[i] += p_begin[0] - '0';
                    p_begin++;
                }
                i++;
            }
            // 清零 resize 可能多分配的尾部
            while (i < data.size())
                data[i++] = 0;
            removeLeadingZero();
        }
        operator std::string() const
        {
            return toString();
        }
        void print() const
        {
            std::cout << toString();
        }
        friend std::istream &operator>>(std::istream &is, Integer &num)
        {
            static std::string tmp;
            tmp.clear();
            is >> tmp;
            num.fromString(tmp);
            return is;
        }
        friend std::ostream &operator<<(std::ostream &os, const Integer &num)
        {
            return os << num.toString();
        }
        static int absCompare(View input1, View input2)
        {
            size_t len1 = count_true_length(input1.ptr, input1.size);
            size_t len2 = count_true_length(input2.ptr, input2.size);
            if (len1 != len2)
            {
                return len1 > len2 ? 1 : -1;
            }
            size_t i = len1;
            while (i > 0)
            {
                i--;
                if (input1[i] != input2[i])
                {
                    return input1[i] > input2[i] ? 1 : -1;
                }
            }
            return 0;
        }
        // 双肢打包 + AVX2 版本 absAdd
        // 16 个 limb 一次处理 (8 个打包 uint32 = 1 个 __m256i)
        // 关键不变量: limb 最大 9999, 两 limb 和最大 19998 < 65536, 不会进位到高 16 位
#pragma GCC push_options
#pragma GCC target("avx2,fma")
        static bool absAdd_avx2(View in1, View in2, Span out)
        {
            if (in1.size < in2.size)
            {
                std::swap(in1, in2);
            }
            size_t i = 0;
            Limb carry = 0;
            // AVX2 主循环: 每次 16 个 limb
            for (; i + 15 < in2.size; i += 16)
            {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
                __m256i r = _mm256_add_epi32(a, b);
                alignas(32) uint32_t tmp[8];
                _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
                // 串行 carry 传播: 8 步, 每步处理 2 个 limb (lo/hi), 无分支掩码
                uint32_t c = carry;
                for (int k = 0; k < 8; k++)
                {
                    uint32_t lo = tmp[k] & 0xFFFF;
                    uint32_t hi = tmp[k] >> 16;
                    lo += c;
                    uint32_t clo = lo >= 10000;
                    lo -= clo * 10000u;
                    hi += clo;
                    uint32_t chi = hi >= 10000;
                    hi -= chi * 10000u;
                    c = chi;
                    tmp[k] = lo | (hi << 16);
                }
                carry = static_cast<Limb>(c);
                __m256i out_vec = _mm256_load_si256(reinterpret_cast<const __m256i *>(tmp));
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);
            }
            // 标量尾部 (8 路展开)
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = add_half<Limb>(in1[i],   in2[i]   + carry, BASE, carry);
                out[i+1] = add_half<Limb>(in1[i+1], in2[i+1] + carry, BASE, carry);
                out[i+2] = add_half<Limb>(in1[i+2], in2[i+2] + carry, BASE, carry);
                out[i+3] = add_half<Limb>(in1[i+3], in2[i+3] + carry, BASE, carry);
                out[i+4] = add_half<Limb>(in1[i+4], in2[i+4] + carry, BASE, carry);
                out[i+5] = add_half<Limb>(in1[i+5], in2[i+5] + carry, BASE, carry);
                out[i+6] = add_half<Limb>(in1[i+6], in2[i+6] + carry, BASE, carry);
                out[i+7] = add_half<Limb>(in1[i+7], in2[i+7] + carry, BASE, carry);
            }
            for (; i < in2.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
            }
            // 第三段: carry=0 时用 memcpy 快速拷贝
            for (; i < in1.size; i++)
            {
                if (carry == 0)
                {
                    break;
                }
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            if (carry == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
            }
            return carry;
        }
        // 双肢打包 + AVX2 版本 absSub
        // 用 bias = BASE|BASE<<16 加到 a 后再减 b, 保证每个 16-bit lane 不下溢
        // r_limb = a_limb + BASE - b_limb, 范围 [1, 19999]
        // r >= BASE -> 无借位, 结果 = r - BASE; r < BASE -> 借位, 结果 = r
        static bool absSub_avx2(View in1, View in2, Span out)
        {
            assert(in1.size >= in2.size);
            size_t i = 0;
            Limb borrow = 0;
            __m256i bias_vec = _mm256_set1_epi32(static_cast<int>(10000u | (10000u << 16)));
            for (; i + 15 < in2.size; i += 16)
            {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
                __m256i r = _mm256_sub_epi32(_mm256_add_epi32(a, bias_vec), b);
                alignas(32) uint32_t tmp[8];
                _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
                // 串行 borrow 传播: 8 步, 每步处理 2 个 limb (lo/hi), 无分支掩码
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
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(out.ptr + i), out_vec);
            }
            // 标量尾部 (8 路展开)
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = sub_half<Limb>(in1[i],   in2[i]   + borrow, BASE, borrow);
                out[i+1] = sub_half<Limb>(in1[i+1], in2[i+1] + borrow, BASE, borrow);
                out[i+2] = sub_half<Limb>(in1[i+2], in2[i+2] + borrow, BASE, borrow);
                out[i+3] = sub_half<Limb>(in1[i+3], in2[i+3] + borrow, BASE, borrow);
                out[i+4] = sub_half<Limb>(in1[i+4], in2[i+4] + borrow, BASE, borrow);
                out[i+5] = sub_half<Limb>(in1[i+5], in2[i+5] + borrow, BASE, borrow);
                out[i+6] = sub_half<Limb>(in1[i+6], in2[i+6] + borrow, BASE, borrow);
                out[i+7] = sub_half<Limb>(in1[i+7], in2[i+7] + borrow, BASE, borrow);
            }
            for (; i < in2.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], in2[i] + borrow, BASE, borrow);
            }
            for (; i < in1.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            return borrow;
        }
#pragma GCC pop_options
        static bool absAdd(View in1, View in2, Span out)
        {
            if (in2.size >= 16)
            {
                return absAdd_avx2(in1, in2, out);
            }
            if (in1.size < in2.size)
            {
                std::swap(in1, in2);
            }
            size_t i = 0;
            Limb carry = 0;
            // 8 路展开 + add_half 无分支掩码 (消除除法, 对齐 best/add.cpp 策略)
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = add_half<Limb>(in1[i],   in2[i]   + carry, BASE, carry);
                out[i+1] = add_half<Limb>(in1[i+1], in2[i+1] + carry, BASE, carry);
                out[i+2] = add_half<Limb>(in1[i+2], in2[i+2] + carry, BASE, carry);
                out[i+3] = add_half<Limb>(in1[i+3], in2[i+3] + carry, BASE, carry);
                out[i+4] = add_half<Limb>(in1[i+4], in2[i+4] + carry, BASE, carry);
                out[i+5] = add_half<Limb>(in1[i+5], in2[i+5] + carry, BASE, carry);
                out[i+6] = add_half<Limb>(in1[i+6], in2[i+6] + carry, BASE, carry);
                out[i+7] = add_half<Limb>(in1[i+7], in2[i+7] + carry, BASE, carry);
            }
            for (; i < in2.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
            }
            // 第三段: carry=0 时用 memcpy 快速拷贝 (避免逐位 add_half 开销)
            for (; i < in1.size; i++)
            {
                if (carry == 0)
                {
                    break;
                }
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            if (carry == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
            }
            return carry;
        }
        static bool absSub(View in1, View in2, Span out)
        {
            if (in2.size >= 16)
            {
                return absSub_avx2(in1, in2, out);
            }
            assert(in1.size >= in2.size);
            size_t i = 0;
            Limb borrow = 0;
            for (; i + 7 < in2.size; i += 8)
            {
                out[i]   = sub_half<Limb>(in1[i],   in2[i]   + borrow, BASE, borrow);
                out[i+1] = sub_half<Limb>(in1[i+1], in2[i+1] + borrow, BASE, borrow);
                out[i+2] = sub_half<Limb>(in1[i+2], in2[i+2] + borrow, BASE, borrow);
                out[i+3] = sub_half<Limb>(in1[i+3], in2[i+3] + borrow, BASE, borrow);
                out[i+4] = sub_half<Limb>(in1[i+4], in2[i+4] + borrow, BASE, borrow);
                out[i+5] = sub_half<Limb>(in1[i+5], in2[i+5] + borrow, BASE, borrow);
                out[i+6] = sub_half<Limb>(in1[i+6], in2[i+6] + borrow, BASE, borrow);
                out[i+7] = sub_half<Limb>(in1[i+7], in2[i+7] + borrow, BASE, borrow);
            }
            for (; i < in2.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], in2[i] + borrow, BASE, borrow);
            }
            for (; i < in1.size; i++)
            {
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            return borrow;
        }
        static bool absAdd1(View in1, Limb in2, Span out)
        {
            assert(in1.size > 0);
            Limb carry = 0;
            out[0] = add_half<Limb>(in1[0], in2, BASE, carry);
            for (size_t i = 1; i < in1.size; i++)
            {
                if (carry == 0)
                {
                    // in-place 时后续 out[i] 已等于 in1[i], 直接返回
                    if (in1.ptr == out.ptr) return false;
                    // 非 in-place 时复制剩余部分
                    std::copy(in1.ptr + i, in1.ptr + in1.size, out.ptr + i);
                    return false;
                }
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            return carry;
        }
        static bool absSub1(View in1, Limb in2, Span out)
        {
            assert(in1.size > 0);
            Limb borrow = 0;
            out[0] = sub_half<Limb>(in1[0], in2, BASE, borrow);
            for (size_t i = 1; i < in1.size; i++)
            {
                if (borrow == 0)
                {
                    if (in1.ptr == out.ptr) return false;
                    std::copy(in1.ptr + i, in1.ptr + in1.size, out.ptr + i);
                    return false;
                }
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            return borrow;
        }
        friend bool operator>(const Integer &input1, const Integer &input2)
        {
            if (input1.isNeg() != input2.isNeg())
            {
                return input2.isNeg();
            }
            return (absCompare(input1.getView(), input2.getView()) > 0) != input1.isNeg();
        }
        friend bool operator<(const Integer &input1, const Integer &input2)
        {
            return input2 > input1;
        }
        friend bool operator>=(const Integer &input1, const Integer &input2)
        {
            return !(input1 < input2);
        }
        friend bool operator<=(const Integer &input1, const Integer &input2)
        {
            return !(input1 > input2);
        }
        friend bool operator==(const Integer &input1, const Integer &input2)
        {
            if (input1.isNeg() != input2.isNeg())
            {
                return false;
            }
            return absCompare(input1.getView(), input2.getView()) == 0;
        }
        friend bool operator!=(const Integer &input1, const Integer &input2)
        {
            return !(input1 == input2);
        }
        Integer &add(View input, bool in_sign)
        {
            bool same = input.ptr == this->getView().ptr;
            size_t len1 = this->length(), len2 = input.size;
            if (this->isNeg() == in_sign) 
            {
                size_t add_len = std::max(len1, len2) + 1;
                this->data.resize(add_len);
                auto view1 = View(this->data.data(), len1), view2 = input;
                if (same)
                {
                    view2 = view1;
                }
                this->data[add_len - 1] = absAdd(view1, view2, this->getSpan());
            }
            else
            {
                if (same)
                {
                    return *this = Integer{};
                }
                size_t sub_len = std::max(len1, len2);
                this->data.resize(sub_len);
                auto view1 = View(this->data.data(), len1), view2 = input;
                int cmp = absCompare(view1, view2);
                if (cmp > 0)
                {
                    
                    absSub(view1, view2, this->getSpan());
                }
                else if (cmp < 0)
                {
                    this->setSign(in_sign);
                    absSub(view2, view1, this->getSpan());
                }
                else
                {
                    this->data.clear();
                }
            }
            this->removeLeadingZero();
            return *this;
        }
        static void basicMul(View in1, View in2, Span out)
        {
            if (in1.size > in2.size)
            {
                std::swap(in1, in2);
            }
            if (in1.size == 0)
            {
                return;
            }
            
            
            thread_local std::vector<Limb> buf;
            size_t buf_size = in1.size + in2.size;
            if (buf.size() < buf_size)
                buf.resize(buf_size);
            Limb carry = 0, x = in1[0];
            for (size_t j = 0; j < in2.size; j++)
            {
                Limb2 prod = Limb2(in2[j]) * x + carry;
                buf[j] = prod % BASE;
                carry = prod / BASE;
            }
            buf[in2.size] = carry;
            for (size_t i = 1; i < in1.size; i++)
            {
                x = in1[i], carry = 0;
                for (size_t j = 0; j < in2.size; j++)
                {
                    Limb2 prod = Limb2(in2[j]) * x + carry + buf[i + j];
                    buf[i + j] = prod % BASE;
                    carry = prod / BASE;
                }
                buf[i + in2.size] = carry;
            }
            std::copy(buf.begin(), buf.begin() + buf_size, out.begin());
        }
        static void fftMul(View in1, View in2, Span out)
        {
            
            
            size_t len1 = count_true_length(in1.ptr, in1.size);
            size_t len2 = count_true_length(in2.ptr, in2.size);
            if (len1 == 0 || len2 == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t conv_len = len1 + len2 - 1, float_len = int_ceil2(conv_len);
            
            thread_local std::vector<double> tv1, tv2;
            if (tv1.size() < float_len)
                tv1.resize(float_len);
            if (tv2.size() < float_len)
                tv2.resize(float_len);
            double *v1 = tv1.data(), *v2 = tv2.data();
            std::copy(in1.ptr, in1.ptr + len1, v1);
            std::fill(v1 + len1, v1 + float_len, 0.0); 
            std::copy(in2.ptr, in2.ptr + len2, v2);
            std::fill(v2 + len2, v2 + float_len, 0.0);
            transform::fft::real_conv(v1, v2, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
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
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v1[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }
        static void fftSqr(View in, Span out)
        {
            
            size_t len = count_true_length(in.ptr, in.size);
            if (len == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t conv_len = len * 2 - 1, float_len = int_ceil2(conv_len);
            
            thread_local std::vector<double> tv;
            if (tv.size() < float_len)
                tv.resize(float_len);
            double *v = tv.data();
            std::copy(in.ptr, in.ptr + len, v);
            std::fill(v + len, v + float_len, 0.0);
            transform::fft::real_conv(v, v, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
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
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }

#ifndef FFT_SQR_THRESHOLD
#define FFT_SQR_THRESHOLD 64
#endif
#ifndef FFT_MUL_THRESHOLD
#define FFT_MUL_THRESHOLD 64
#endif
#ifndef FFT_MUL_UNBALANCED_MIN
#define FFT_MUL_UNBALANCED_MIN 16384
#endif
#ifndef FFT_MUL_UNBALANCED_RATIO
#define FFT_MUL_UNBALANCED_RATIO 6
#endif
        static void absSqr(View in, Span out)
        {
            assert(out.size >= in.size * 2);
            if (in.size <= FFT_SQR_THRESHOLD)
            {
                basicMul(in, in, out);
            }
            else
            {
                fftSqr(in, out);
            }
        }
        static void fftMulUnbalanced(View large, View small, Span out)
        {
            size_t len_big = count_true_length(large.ptr, large.size);
            size_t len_sml = count_true_length(small.ptr, small.size);
            if (len_big == 0 || len_sml == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t chunk = small.size;
            size_t n_chunks = (len_big + chunk - 1) / chunk;
            size_t float_len = int_ceil2(chunk + chunk - 1);
            
            thread_local std::vector<double> b_dft;
            if (b_dft.size() < float_len) b_dft.resize(float_len);
            prepareDFT(small, b_dft.data(), float_len);
            
            // 保存大数数据副本（large.ptr 可能和 out.ptr 指向同一缓冲区）
            thread_local std::vector<Limb> large_copy;
            if (large_copy.size() < len_big) large_copy.resize(len_big);
            std::copy_n(large.ptr, len_big, large_copy.data());
            
            std::fill_n(out.ptr, out.size, Limb(0));
            
            thread_local std::vector<Limb> tbuf;
            size_t tbuf_max = chunk + chunk;
            if (tbuf.size() < tbuf_max) tbuf.resize(tbuf_max);
            
            for (size_t ci = 0; ci < n_chunks; ci++)
            {
                size_t offset = ci * chunk;
                size_t this_chunk = std::min(chunk, len_big - offset);
                size_t this_conv = this_chunk + chunk - 1;
                
                View chunk_view(large_copy.data() + offset, this_chunk);
                Span temp_span(tbuf.data(), this_conv + 1);
                fftMulPre(chunk_view, b_dft.data(), chunk, float_len, temp_span);
                
                uint64_t carry = 0;
                size_t wpos = offset;
                for (size_t i = 0; i < this_conv; i++)
                {
                    carry += uint64_t(out[wpos]) + tbuf[i];
                    out[wpos] = Limb(carry % BASE);
                    carry /= BASE;
                    wpos++;
                }
                carry += tbuf[this_conv];
                while (carry > 0 && wpos < out.size)
                {
                    carry += out[wpos];
                    out[wpos] = Limb(carry % BASE);
                    carry /= BASE;
                    wpos++;
                }
            }
        }
        static void absMul(View in1, View in2, Span out)
        {
            assert(out.size >= in1.size + in2.size);
            if (in1.ptr == in2.ptr)
            {
                absSqr(in1, out);
                return;
            }
            size_t sml = std::min(in1.size, in2.size);
            if (sml <= FFT_MUL_THRESHOLD)
            {
                basicMul(in1, in2, out);
            }
            else
            {
                size_t big = std::max(in1.size, in2.size);
                if (sml >= FFT_MUL_UNBALANCED_MIN && big >= sml * FFT_MUL_UNBALANCED_RATIO)
                {
                    if (in1.size >= in2.size)
                        fftMulUnbalanced(in1, in2, out);
                    else
                        fftMulUnbalanced(in2, in1, out);
                }
                else
                {
                    fftMul(in1, in2, out);
                }
            }
        }
        
        
        static void prepareDFT(View in, double *dft_buf, size_t float_len)
        {
            assert(is_2pow(float_len));
            assert(float_len >= in.size);
            std::copy(in.begin(), in.end(), dft_buf);
            std::fill(dft_buf + in.size, dft_buf + float_len, 0.0);
            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(float_len);
            fft.template dif<true>(dft_buf, float_len);
        }
        
        
        static void fftMulPre(View a, const double *b_dft, size_t b_len, size_t float_len, Span out)
        {
            
            
            size_t a_len = count_true_length(a.ptr, a.size);
            if (a_len == 0)
            {
                std::fill_n(out.ptr, out.size, Limb(0));
                return;
            }
            size_t conv_len = a_len + b_len - 1;
            assert(float_len >= conv_len);
            thread_local std::vector<double> tv;
            if (tv.size() < float_len)
                tv.resize(float_len);
            double *v = tv.data();
            std::copy(a.ptr, a.ptr + a_len, v);
            std::fill(v + a_len, v + float_len, 0.0);
            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(float_len);
            fft.template dif<true>(v, float_len);
            transform::fft::real_dot_binrev2(v, b_dft, float_len);
            fft.template idit<true>(v, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                // Barrett: q=divBASE(s); out=s-q*BASE; next_s=q+v[i+1]
                // was: s1 = s0 / BASE + ...; out[i] = s0 % BASE; carry = s7 / BASE;
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
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
            for (; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            out[conv_len] = Limb(carry);

            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
        }

        // fftMulModBm1: 计算 a * b mod (B^m - 1), 结果长度 m (cyclic convolution)
        // m 必须是 2 的幂, 且 a_len <= m, b_len <= m
        // 用途: absInvNewton GMP 风格两步分解, FFT 长度从 ceil2(2m-1) 降到 m
        // 算法: cyclic FFT (float_len=m) + 进位传播 + cyclic carry 折叠 (B^m ≡ 1 mod B^m-1)
        static void fftMulModBm1(View a, View b, size_t m, Span out)
        {
            assert(is_2pow(m));
            assert(out.size >= m);
            size_t a_len = count_true_length(a.ptr, a.size);
            size_t b_len = count_true_length(b.ptr, b.size);
            if (a_len == 0 || b_len == 0)
            {
                std::fill_n(out.ptr, m, Limb(0));
                if (out.size > m)
                    std::fill_n(out.ptr + m, out.size - m, Limb(0));
                return;
            }
            assert(a_len <= m && b_len <= m);

            thread_local std::vector<double> tv;
            if (tv.size() < 2 * m)
                tv.resize(2 * m);
            double *va = tv.data();
            double *vb = tv.data() + m;

            std::copy(a.ptr, a.ptr + a_len, va);
            std::fill(va + a_len, va + m, 0.0);
            std::copy(b.ptr, b.ptr + b_len, vb);
            std::fill(vb + b_len, vb + m, 0.0);

            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(m);
            fft.template dif<true>(va, m);
            fft.template dif<true>(vb, m);
            transform::fft::real_dot_binrev2(va, vb, m);
            fft.template idit<true>(va, m);

            // 进位传播 (8 路展开, 同 fftMulPre)
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < m; i += 8)
            {
                uint64_t s0 = carry + uint64_t(va[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(va[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(va[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(va[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(va[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(va[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(va[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(va[i+7] + 0.5);
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
            for (; i < m; i++)
            {
                carry += uint64_t(va[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }

            // cyclic 折叠: B^m ≡ 1 (mod B^m-1), 所以 out_final = (out + carry) mod (B^m-1)
            // 进位传播 carry 到 out, 最多绕 1 圈 (carry 衰减极快)
            while (carry > 0)
            {
                bool wrapped = true;
                for (size_t j = 0; j < m && carry > 0; j++)
                {
                    uint64_t s = uint64_t(out[j]) + carry;
                    uint64_t q = divBASE(s);
                    out[j] = Limb(s - q * BASE);
                    carry = q;
                    if (carry == 0) { wrapped = false; break; }
                }
                // 若绕完整一圈且 carry 仍 > 0, 说明 sum(out) + carry >= B^m
                // (sum(out) + carry) mod (B^m-1) = (sum(out_new) + carry*B^m) mod (B^m-1)
                //   = (sum(out_new) + carry) mod (B^m-1)   [因为 B^m ≡ 1]
                // 实际中 carry 经第一轮后 < BASE, 第二轮必收敛, 最多 2 轮
                if (wrapped && carry > 0)
                {
                    // carry 应 = 1 (sum(out_old)+carry_initial < 2*B^m)
                    // 结果 = out_new + 1 (mod B^m-1), 不是 0!
                    // 旧代码填 0 是 bug: B^m mod (B^m-1) = 1, 不是 0
                    if (carry == 1)
                    {
                        // out += 1, cyclic carry (若全 BASE-1, 结果 = 0)
                        Limb c = 1;
                        for (size_t j = 0; j < m; j++)
                        {
                            if (out[j] + c < BASE) { out[j] = Limb(out[j] + c); c = 0; break; }
                            else { out[j] = Limb(out[j] + c - BASE); }
                        }
                        carry = 0;
                    }
                    // carry > 1 理论上不会发生, 但防御性继续 while
                }
            }

            if (out.size > m)
                std::fill_n(out.ptr + m, out.size - m, Limb(0));
        }

        // fftMulModBm1Pre: fftMulModBm1 的预计算 DFT 版本 (b 的 DFT 预先计算)
        // b_dft 长度 = m, 由 prepareDFT(b, b_dft, m) 预计算
        static void fftMulModBm1Pre(View a, const double *b_dft, size_t b_len, size_t m, Span out)
        {
            assert(is_2pow(m));
            assert(out.size >= m);
            size_t a_len = count_true_length(a.ptr, a.size);
            if (a_len == 0)
            {
                std::fill_n(out.ptr, m, Limb(0));
                if (out.size > m)
                    std::fill_n(out.ptr + m, out.size - m, Limb(0));
                return;
            }
            assert(a_len <= m);

            thread_local std::vector<double> tv;
            if (tv.size() < m)
                tv.resize(m);
            double *v = tv.data();

            std::copy(a.ptr, a.ptr + a_len, v);
            std::fill(v + a_len, v + m, 0.0);

            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(m);
            fft.template dif<true>(v, m);
            transform::fft::real_dot_binrev2(v, b_dft, m);
            fft.template idit<true>(v, m);

            // 进位传播 (cyclic mod B^m-1) — 同 fftMulModBm1
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < m; i += 8)
            {
                uint64_t s0 = carry + uint64_t(v[i]   + 0.5);
                uint64_t q0 = divBASE(s0);
                uint64_t s1 = q0 + uint64_t(v[i+1] + 0.5);
                uint64_t q1 = divBASE(s1);
                uint64_t s2 = q1 + uint64_t(v[i+2] + 0.5);
                uint64_t q2 = divBASE(s2);
                uint64_t s3 = q2 + uint64_t(v[i+3] + 0.5);
                uint64_t q3 = divBASE(s3);
                uint64_t s4 = q3 + uint64_t(v[i+4] + 0.5);
                uint64_t q4 = divBASE(s4);
                uint64_t s5 = q4 + uint64_t(v[i+5] + 0.5);
                uint64_t q5 = divBASE(s5);
                uint64_t s6 = q5 + uint64_t(v[i+6] + 0.5);
                uint64_t q6 = divBASE(s6);
                uint64_t s7 = q6 + uint64_t(v[i+7] + 0.5);
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
            for (; i < m; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                uint64_t q = divBASE(carry);
                out[i] = Limb(carry - q * BASE);
                carry = q;
            }
            while (carry > 0)
            {
                bool wrapped = true;
                for (size_t j = 0; j < m && carry > 0; j++)
                {
                    uint64_t s = uint64_t(out[j]) + carry;
                    uint64_t q = divBASE(s);
                    out[j] = Limb(s - q * BASE);
                    carry = q;
                    if (carry == 0) { wrapped = false; break; }
                }
                if (wrapped && carry > 0)
                {
                    if (carry == 1)
                    {
                        Limb c = 1;
                        for (size_t j = 0; j < m; j++)
                        {
                            if (out[j] + c < BASE) { out[j] = Limb(out[j] + c); c = 0; break; }
                            else { out[j] = Limb(out[j] + c - BASE); }
                        }
                        carry = 0;
                    }
                }
            }

            if (out.size > m)
                std::fill_n(out.ptr + m, out.size - m, Limb(0));
        }
        Integer &square()
        {
            this->setSign(false);
            size_t len = this->length();
            this->data.resize(len * 2);
            absSqr(View(this->data.data(), len), this->getSpan());
            this->removeLeadingZero();
            return *this;
        }
        static Limb absMul1(View in, Limb x, Span out)
        {
            Limb carry = 0;
            for (size_t i = 0; i < in.size; i++)
            {
                Limb2 prod = Limb2(in[i]) * x + carry;
                out[i] = prod % BASE;
                carry = prod / BASE;
            }
            return carry;
        }
        static Limb absDiv1(View in, Limb x, Span out)
        {
            Limb rem = 0;
            size_t i = in.size;
            while (i > 0)
            {
                i--;
                Limb2 prod = Limb2(in[i]) + Limb2(rem) * BASE;
                out[i] = prod / x;
                rem = prod % x;
            }
            return rem;
        }
        Limb selfDivRem1(Limb x)
        {
            Limb rem = absDiv1(this->getView(), x, this->getSpan());
            this->removeLeadingZero();
            return rem;
        }
        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
            size_t quot_idx = len1 - len2;
            
            thread_local std::vector<Limb> tprod;
            if (tprod.size() < len2 + 1)
                tprod.resize(len2 + 1);
            while (quot_idx > 0)
            {
                quot_idx--;
                len1 = quot_idx + len2;
                Limb high1 = dividend[len1], high2 = dividend[len1 - 1], qhat = 0;
                
                if (high1 >= divisor_high)
                {
                    qhat = BASE - 1;
                }
                else
                {
                    Limb2 high = Limb2(high1) * BASE + high2;
                    qhat = high / divisor_high;
                }
                Span prod_span(tprod.data(), len2 + 1);
                prod_span[len2] = absMul1(divisor, qhat, prod_span);
                if (prod_span[len2] == 0)
                {
                    prod_span.size = len2;
                }
                Span dividend_span(dividend + quot_idx);
                int count = 0;
                while (absCompare(View(prod_span), View(dividend_span)) > 0)
                {
                    assert(count < 2);
                    count++;
                    auto bf = absSub(prod_span, divisor, prod_span);
                    qhat--;
                    assert(!bf);
                }
                auto bf = absSub(dividend_span, prod_span, dividend_span);
                assert(!bf);
                quotient[quot_idx] = qhat;
                dividend.size = len1;
            }
        }

        
        // B-1 优化: m_dft/m_dft_float_len 为预计算的 m 的 DFT（可选）
        // 最外层可传入以省 1 次 DFT(m)；递归层不传（m+s 子段 DFT 未预计算）
        // absInvNewton base case 阈值: k<=阈值时走 O(k^2) 学校除法
        // 理论分析: k=128 时 FFT 路径 ~10750 ops vs base case ~16384 ops, FFT 更快
        // k=64 时 FFT 路径 ~4000 ops vs base case ~4096 ops, 接近平衡
        // 当前默认 64 (保守), 可通过 -DINV_NEWTON_BASE_THRESHOLD=128 实验
#ifndef INV_NEWTON_BASE_THRESHOLD
#define INV_NEWTON_BASE_THRESHOLD 64
#endif
        static void absInvNewton(View m, Span inv,
                                 const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;
            assert(k > 0);
            assert(inv.size >= k + 1);
            if (k <= INV_NEWTON_BASE_THRESHOLD)
            {
                Limb b_2k[INV_NEWTON_BASE_THRESHOLD * 2 + 2];
                b_2k[k * 2] = 1;
                std::fill_n(b_2k, k * 2, Limb(0));
                absDivBasicCore(Span(b_2k, k * 2 + 1), m, inv);
                return;
            }
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            size_t s = (k - 1) / 2;
            
            absInvNewton(m + s, inv);    
            size_t inv0_len = k - s + 1; 
            Span inv0(inv.ptr, inv0_len);
            
            
            thread_local std::vector<Limb> tprod, tinv2;
            size_t prod_size = inv0_len * 2 + k;
            size_t inv2_size = k + 1;
            if (tprod.size() < prod_size) tprod.resize(prod_size);
            if (tinv2.size() < inv2_size) tinv2.resize(inv2_size);
            std::fill_n(tinv2.data(), s, Limb(0)); 
            Span prod_span(tprod.data(), prod_size), inv2_span(tinv2.data(), inv2_size);
            bool cf = absAdd(inv0, inv0, inv2_span + s); 
            assert(!cf);                                 
#ifdef PROFILE_DIV
            auto _inv_t0 = std::chrono::high_resolution_clock::now();
#endif
            absSqr(inv0, prod_span);
#ifdef PROFILE_DIV
            auto _inv_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof]   absInvNewton(k=%zu): absSqr (inv0_len=%zu): %.3f ms\n",
                    k, inv0_len, std::chrono::duration<double, std::milli>(_inv_t1 - _inv_t0).count());
            auto _inv_t2 = std::chrono::high_resolution_clock::now();
#endif
            // B-1 优化: 若有预计算的 m DFT 且 float_len 匹配，用 fftMulPre 省 1 次 DFT(m)
            {
                size_t conv_len = inv0_len * 2 + k - 1;
                size_t need_float_len = int_ceil2(conv_len);
                if (m_dft != nullptr && m_dft_float_len == need_float_len)
                {
                    fftMulPre(View(tprod.data(), inv0_len * 2), m_dft, k, need_float_len, prod_span);
                }
                else
                {
                    absMul(View(tprod.data(), inv0_len * 2), m, prod_span);
                }
            }
#ifdef PROFILE_DIV
            auto _inv_t3 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof]   absInvNewton(k=%zu): absMul/fftMulPre (conv_len=%zu, fl=%zu): %.3f ms\n",
                    k, inv0_len * 2 + k - 1, int_ceil2(inv0_len * 2 + k - 1),
                    std::chrono::duration<double, std::milli>(_inv_t3 - _inv_t2).count());
#endif
            prod_span = prod_span + 2 * (k - s);        
            assert(prod_span[prod_span.size - 1] == 0); 
            prod_span.size--;                           
            absSub(inv2_span, prod_span, inv);          
        }

        // GMP 风格的 Newton 逆计算 (两步分解 + cyclic convolution mod B^mn-1)
        // 设计文档: FFT_MULMOD_INTEGRATION_DESIGN.md
        // 当前版本: 仅 fallback 路径 (与 absInvNewton 等价的 HIGH-half 提取法)
        // cyclic 路径留作 TODO, 后续逐步实现 (正剩余类优先, 负剩余类用 assert 捕获)
        static void absInvNewtonGMP(View m, Span inv,
                                    const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;  // GMP n
            assert(k > 0);
            assert(inv.size >= k + 1);

            // base case: 与 absInvNewton 一致, 走 O(k^2) 学校除法
            if (k <= INV_NEWTON_BASE_THRESHOLD)
            {
                Limb b_2k[INV_NEWTON_BASE_THRESHOLD * 2 + 2];
                b_2k[k * 2] = 1;
                std::fill_n(b_2k, k * 2, Limb(0));
                absDivBasicCore(Span(b_2k, k * 2 + 1), m, inv);
                return;
            }

            size_t s = (k - 1) / 2;
            // 递归调用自身 (后续 cyclic 路径启用后, 递归层也会走 GMP 风格)
            absInvNewtonGMP(m + s, inv);
            size_t rn = k - s;        // GMP rn
            size_t inv0_len = rn + 1;
            Span inv0(inv.ptr, inv0_len);

            // === mn 选择 (cyclic convolution 长度) ===
            // 约束: fftMulModBm1 要求 m 是 2 幂
            // 生效条件: mn >= k+1 (容纳 n+1) 且 mn <= k+rn (GMP ASSERT(n >= mn-rn))
            // 阈值: k >= CYCLIC_MIN_K 时启用, 小 k 走 fallback (精度不足风险)
            //   实测: k=112/2001 有数据相关 FAIL (b_top=4/8), k>=6250 benchmark 全 PASS
            //   阈值 4096 平衡: 小 k 性能收益小, 风险高; 大 k 收益大, 已验证安全
#ifndef CYCLIC_MIN_K
#define CYCLIC_MIN_K 4096
#endif
            size_t mn = int_ceil2(k + 1);
#ifndef DISABLE_2NXN_CYCLIC
            bool use_cyclic = (mn >= k + 1) && (mn <= k + rn) && (k >= CYCLIC_MIN_K);
#else
            bool use_cyclic = false;
#endif

            if (use_cyclic)
            {
                // === cyclic 路径: 验证版本 ===
                // 目标: 验证 fftMulModBm1 + GMP 修正的正确性
                // 当前: cyclic 计算 + full mul 验证 + 原组合逻辑 (无性能提升, 但保证正确性)
                // 后续: 实现 GMP 组合逻辑以获得性能提升 (见 FFT_MULMOD_INTEGRATION_DESIGN.md)

                // --- 第一步: cyclic convolution ---
                // xp_mod[0..mn-1] = (inv0 * m) mod (B^mn - 1)
                thread_local std::vector<Limb> txp_mod;
                if (txp_mod.size() < mn + 2) txp_mod.resize(mn + 2);
                std::fill_n(txp_mod.data(), mn + 2, Limb(0));
                Span xp_mod(txp_mod.data(), mn);

                if (m_dft != nullptr && m_dft_float_len == mn)
                {
                    fftMulModBm1Pre(inv0, m_dft, k, mn, xp_mod);
                }
                else
                {
                    fftMulModBm1(inv0, m, mn, xp_mod);
                }

                // --- GMP 修正: xp = (xp - B^{rn+k}) mod (B^mn - 1) ---
                // 注: GMP 源码 L176-L182 是 (ip*dp + dp*B^rn - B^{rn+n}) mod B^mn-1
                //     其中 GMP 的 ip 是 rn 位小数部分, dp*B^rn 用于凑成 inv0 = ip + B^rn
                // 本项目 inv0 已是 rn+1 位 (含 B^rn 整数位), fftMulModBm1(inv0, m, ...) 直接得 inv0*m
                // 因此只需减 B^{rn+k} = B^{rn+n}, 即 X = inv0*m - B^{rn+k}, 满足 |2X| < B^mn-1
                //    B^{rn+k} mod (B^mn-1) = B^{(rn+k) mod mn} = B^{rn+k-mn} (因为 mn <= rn+k < 2*mn)
                //    在位置 (rn+k-mn) 减 1, 处理借位 (mod B^mn-1, 可能 wrap)
                {
                    size_t sub_pos = rn + k - mn;
                    assert(sub_pos < mn);
                    Limb borrow = 1;
                    size_t j = sub_pos;
                    size_t guard = 0; // 防死循环
                    while (borrow > 0 && guard < 2 * mn)
                    {
                        if (xp_mod[j] >= borrow)
                        {
                            xp_mod[j] -= borrow;
                            borrow = 0;
                        }
                        else
                        {
                            xp_mod[j] = Limb(xp_mod[j] + BASE - borrow);
                            borrow = 1;
                            j = (j + 1) % mn;
                        }
                        guard++;
                    }
                    // 若绕一圈仍有 borrow, 说明 xp_mod 原 = 0, 减 1 = -1 mod (B^mn-1) = B^mn-2
                    // 这是负剩余类的情况, xp_mod[k] 应为 BASE-1
                    if (borrow > 0)
                    {
                        // xp_mod = -1 mod (B^mn-1) = B^mn - 2
                        // 表示为 [BASE-2, BASE-1, ..., BASE-1] (低位 BASE-2, 高位 BASE-1)
                        std::fill_n(xp_mod.ptr, mn, Limb(BASE - 1));
                        xp_mod[0] = Limb(BASE - 2);
                    }
                }

                // c) 正/负剩余类判定
                //    xp_mod[k] < 2 → 正剩余类 (X >= 0)
                //    xp_mod[k] >= BASE-2 → 负剩余类 (X < 0)
                Limb xpn = xp_mod[k];
                bool is_positive;
                if (xpn < 2)
                {
                    is_positive = true;
                }
                else if (xpn >= BASE - 2)
                {
                    is_positive = false;
                }
                else
                {
                    // 异常: 既不是正也不是负剩余类
                    // 可能原因: FFT 浮点精度误差, 或 mn 选择导致 |2X| >= B^mn-1
                    // 安全回退到 fallback (与 absInvNewton 一致)
                    is_positive = false;
                    goto gmp_newton_fallback;
                }

                // === 阶段 B: GMP 两步分解 (cyclic + mul_n + 组合) ===
                // 仅处理正剩余类; 负剩余类回退到 fallback (full mul 组合, 与 absInvNewton 一致)
                if (is_positive)
                {
                    // 1. 把 inv0 从 inv.ptr[0..rn] 移到 inv.ptr[s..k] (与 GMP 布局对齐)
                    //    移动后: inv.ptr[s..k-1] = inv0 低 rn 位, inv.ptr[k] = 1 (整数位)
                    std::copy_backward(inv.ptr, inv.ptr + rn + 1, inv.ptr + k + 1);

                    // 2. 分配 xp_full 缓冲区 (长度 2k+2, 初始清零)
                    thread_local std::vector<Limb> txp_full;
                    if (txp_full.size() < 2 * k + 2) txp_full.resize(2 * k + 2);
                    std::fill_n(txp_full.data(), 2 * k + 2, Limb(0));

                    // 3. 把 cyclic 结果 xp_mod[0..mn-1] 复制到 xp_full[0..mn-1]
                    std::copy(xp_mod.ptr, xp_mod.ptr + mn, txp_full.data());

                    Limb *xp = txp_full.data();
                    Limb cy = 0;  // cyclic 模式 cy=0 (GMP L183)

                    // 4a. GMP L188-L199: 正剩余类修正
                    //   cy = xp[n]; if (cy++ && !sub_n) { ASSERT_CARRY(sub_n); ++cy; }
                    cy = xp[k];  // 0 or 1
                    if (cy > 0)
                    {
                        // xp[0..k-1] -= m[0..k-1] (仅低 k 位, 不动 xp[k])
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        cy = 2;  // cy++ 后
                        if (!borrow)
                        {
                            // borrow=0 表示 xp_low >= m, 再减一次 (ASSERT_CARRY: 必须借位)
                            borrow = absSub(View(xp, k), m, Span(xp, k));
                            assert(borrow && "ASSERT_CARRY: second sub must borrow");
                            cy = 3;
                        }
                    }
                    else
                    {
                        cy = 1;  // cy++ 后
                    }

                    // 4b. GMP L211: if (mpn_cmp(xp, dp-n, n) > 0) { sub_n; ++cy; }
                    if (absCompare(View(xp, k), m) > 0)
                    {
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        assert(!borrow && "ASSERT_NOCARRY");
                        cy++;
                    }

                    // 4c. GMP L215: sub_nc(xp_high, dp-rn, xp+n-rn, rn, borrow_in)
                    //   xp_high = m[s..k-1] - xp[s..k-1] - borrow_in
                    //   borrow_in = (xp[0..s-1] > m[0..s-1]) ? 1 : 0
                    //   注: dp-rn 对应 m.ptr+s, xp+n-rn 对应 xp+s
                    assert(2 * k >= 3 * rn && "xp_high and mul_out must not overlap");
                    Limb *xp_high = xp + 2 * k - rn;
                    Limb bf = (absCompare(View(xp, s), View(m.ptr, s)) > 0) ? 1 : 0;
                    for (size_t i = 0; i < rn; i++)
                    {
                        xp_high[i] = sub_half<Limb>(m.ptr[s + i], xp[s + i] + bf, BASE, bf);
                    }
                    assert(bf == 0 && "ASSERT_NOCARRY: xp_high sub overflow");

                    // 4d. GMP L217: MPN_DECR_U(ip-rn, rn, cy)
                    //   inv0_low_rn (inv.ptr[s..k-1]) -= cy
                    {
                        bool bf2 = absSub1(View(inv.ptr + s, rn), cy, Span(inv.ptr + s, rn));
                        assert(!bf2 && "MPN_DECR_U underflow");
                    }

                    // 5. GMP L228: mul_n: xp[0..2rn-1] = xp_high * inv0_low_rn
                    //   xp_high = xp[2k-rn..2k-1] (rn 位), inv0_low_rn = inv.ptr[s..k-1] (rn 位)
                    {
                        View xp_high_view(xp + 2 * k - rn, rn);
                        View inv0_low_view(inv.ptr + s, rn);
                        Span mul_out(xp, 2 * rn);
                        absMul(xp_high_view, inv0_low_view, mul_out);
                    }

                    // 6. GMP L229-L231: 组合
                    // L229: xp[rn..rn+add_len-1] += xp_high[0..add_len-1] (add_len = 2rn-n)
                    size_t add_len = 2 * rn - k;
                    Limb carry = 0;
                    if (add_len > 0)
                    {
                        carry = absAdd(View(xp + rn, add_len),
                                       View(xp + 2 * k - rn, add_len),
                                       Span(xp + rn, add_len)) ? 1 : 0;
                    }

                    // L230: inv.ptr[0..s-1] = xp[3rn-k..2rn-1] + xp_high[2rn-k..rn-1] + carry
                    //   注: xp[3rn-k..2rn-1] 是 mul_n 输出高位, xp_high[2rn-k..rn-1] 是 xp_high 高 s 位
                    {
                        Limb *xph = xp + 2 * k - rn;
                        for (size_t i = 0; i < s; i++)
                        {
                            Limb2 sum = Limb2(xp[3 * rn - k + i]) + Limb2(xph[2 * rn - k + i]) + carry;
                            inv.ptr[i] = Limb(sum % BASE);
                            carry = Limb(sum / BASE);
                        }
                    }

                    // L231: MPN_INCR_U(ip-rn, rn, carry)
                    //   inv.ptr[s..k-1] += carry (进位传播)
                    if (carry > 0)
                    {
                        bool cf2 = absAdd1(View(inv.ptr + s, rn), carry, Span(inv.ptr + s, rn));
                        assert(!cf2 && "MPN_INCR_U overflow into integer bit");
                    }

                    return;
                }
                // 负剩余类或异常: 回退到 fallback (full mul 组合逻辑, 与 absInvNewton 一致)
            }

        gmp_newton_fallback:
            // === fallback: 原 absInvNewton 的 HIGH-half 提取法 ===
            // Newton 一步: inv = 2*inv0 - (inv0^2 * m)_high
            // 注: 此分支与 absInvNewton 逻辑完全一致, 用于保证正确性
            // 触发条件: (1) use_cyclic=false (2) 负剩余类 (3) 异常剩余类 (FFT 精度/mn 选择)
            thread_local std::vector<Limb> tprod, tinv2;
            size_t prod_size = inv0_len * 2 + k;
            size_t inv2_size = k + 1;
            if (tprod.size() < prod_size) tprod.resize(prod_size);
            if (tinv2.size() < inv2_size) tinv2.resize(inv2_size);
            std::fill_n(tinv2.data(), s, Limb(0));
            Span prod_span(tprod.data(), prod_size), inv2_span(tinv2.data(), inv2_size);
            bool cf = absAdd(inv0, inv0, inv2_span + s);
            assert(!cf);
            absSqr(inv0, prod_span);
            // B-1 优化: 若有预计算的 m DFT 且 float_len 匹配, 用 fftMulPre 省 1 次 DFT(m)
            // 注意: cyclic 路径下 m_dft 的 float_len 应为 mn, 与这里 need_float_len 不一致
            //       所以 float_len 不匹配时不复用 (保持与 absInvNewton 一致的行为)
            {
                size_t conv_len = inv0_len * 2 + k - 1;
                size_t need_float_len = int_ceil2(conv_len);
                if (m_dft != nullptr && m_dft_float_len == need_float_len)
                {
                    fftMulPre(View(tprod.data(), inv0_len * 2), m_dft, k, need_float_len, prod_span);
                }
                else
                {
                    absMul(View(tprod.data(), inv0_len * 2), m, prod_span);
                }
            }
            prod_span = prod_span + 2 * (k - s);
            assert(prod_span[prod_span.size - 1] == 0);
            prod_span.size--;
            absSub(inv2_span, prod_span, inv);
        }
        static void absDivNewtonWithInv(Span dividend, View divisor, Span quotient, View inv_span)
        {
            assert(dividend.size <= divisor.size * 2);
            if (dividend.size <= divisor.size)
            {
                return;
            }
            size_t k = divisor.size;
            Span divid_high = dividend + (k - 1);
            
            thread_local std::vector<Limb> tqhat, tprod;
            size_t qhat_len = divid_high.size + inv_span.size;
            size_t prod_len = qhat_len - 1;
            if (tqhat.size() < qhat_len)
                tqhat.resize(qhat_len);
            if (tprod.size() < prod_len)
                tprod.resize(prod_len);
            
            Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
            absMul(inv_span, divid_high, qhat_span); 
            qhat_span = qhat_span + (k + 1);         
            absMul(divisor, qhat_span, prod_span);   
            prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
            
            while (absCompare(prod_span, dividend) > 0)
            {
                absSub(prod_span, divisor, prod_span); 
                absSub1(qhat_span, 1, qhat_span);      
            }
            absSub(dividend, prod_span, dividend); 
            dividend.size = k;
            while (absCompare(dividend, divisor) >= 0)
            {
                absSub(dividend, divisor, dividend);
                absAdd1(qhat_span, 1, qhat_span);
            }
            assert(qhat_span[qhat_span.size - 1] == 0);
            qhat_span.size--;
            std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
        }
        
        
        static void absDivNewtonWithInvFast(Span dividend, View divisor, Span quotient, View inv_span, const double *inv_dft, size_t inv_float_len, const double *divisor_dft, size_t divisor_float_len)
        {
            assert(dividend.size <= divisor.size * 2);
            if (dividend.size <= divisor.size)
            {
                return;
            }
            size_t k = divisor.size;
            Span divid_high = dividend + (k - 1);
            thread_local std::vector<Limb> tqhat, tprod;
            size_t qhat_len = divid_high.size + inv_span.size;
            size_t prod_len = qhat_len - 1;
            if (tqhat.size() < qhat_len)
                tqhat.resize(qhat_len);
            if (tprod.size() < prod_len)
                tprod.resize(prod_len);
            Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
            fftMulPre(divid_high, inv_dft, inv_span.size, inv_float_len, qhat_span);
            qhat_span = qhat_span + (k + 1);
            fftMulPre(qhat_span, divisor_dft, divisor.size, divisor_float_len, prod_span);
            // OPT: 删除 count_true_length — absCompare 内部会重算
            while (absCompare(prod_span, dividend) > 0)
            {
                absSub(prod_span, divisor, prod_span);
                absSub1(qhat_span, 1, qhat_span);
            }
            // OPT: 修正后 prod <= dividend, 真实长度 <= dividend.size, 截断后保证 absSub 安全
            prod_span.size = std::min(prod_span.size, dividend.size);
            absSub(dividend, prod_span, dividend);
            dividend.size = k;
            while (absCompare(dividend, divisor) >= 0)
            {
                absSub(dividend, divisor, dividend);
                absAdd1(qhat_span, 1, qhat_span);
            }
             assert(qhat_span[qhat_span.size - 1] == 0);
             qhat_span.size--;
             std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
         }
         
         // 宽松分块除法: qhat 可能有更大误差，但通过更多修正循环补偿
         // 用于低精度逆时降低 qhat 估计的压力
         static void absDivNewtonWithInvLoose(Span dividend, View divisor, Span quotient, View inv_span)
         {
             assert(dividend.size <= divisor.size * 2);
             if (dividend.size <= divisor.size)
             {
                 return;
             }
             size_t k = divisor.size;
             Span divid_high = dividend + (k - 1);
             
             thread_local std::vector<Limb> tqhat, tprod;
             size_t qhat_len = divid_high.size + inv_span.size;
             size_t prod_len = qhat_len - 1;
             if (tqhat.size() < qhat_len)
                 tqhat.resize(qhat_len);
             if (tprod.size() < prod_len)
                 tprod.resize(prod_len);
             
             Span qhat_span(tqhat.data(), qhat_len), prod_span(tprod.data(), prod_len);
             absMul(inv_span, divid_high, qhat_span); 
             qhat_span = qhat_span + (k + 1);         
             absMul(divisor, qhat_span, prod_span);   
             prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
             
             // 修正循环: 允许多达 5 次迭代（增加容错能力）
             int corrections = 0;
             while (absCompare(prod_span, dividend) > 0 && corrections < 5)
             {
                 absSub(prod_span, divisor, prod_span); 
                 absSub1(qhat_span, 1, qhat_span);
                 corrections++;      
             }
             absSub(dividend, prod_span, dividend); 
             dividend.size = k;
             // 最终检查: 同样允许多次修正
             corrections = 0;
             while (absCompare(dividend, divisor) >= 0 && corrections < 5)
             {
                 absSub(dividend, divisor, dividend);
                 absAdd1(qhat_span, 1, qhat_span);
                 corrections++;
             }
             assert(qhat_span[qhat_span.size - 1] == 0);
             qhat_span.size--;
             std::copy(qhat_span.begin(), qhat_span.end(), quotient.begin());
         }
         
         static void absDivNewtonCore1(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size || dividend.size >= divisor.size * 2)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size, quot_len = len1 - len2, shift_len = len2 - quot_len;
            assert(divisor[len2 - 1] >= HALF_BASE);
            Span dividend_high = dividend + shift_len;
            View divisor_high = divisor + shift_len;
            if (absCompare(dividend_high + quot_len, divisor_high) >= 0)
            {
                std::fill_n(quotient.begin(), quot_len, Limb(BASE - 1));
                
                
                
                absSub(dividend_high + quot_len, divisor_high, dividend_high + quot_len);
                dividend_high[quot_len] = absAdd(dividend_high, divisor_high, dividend_high);
            }
            else
            {
                thread_local std::vector<Limb> t_inv;
                size_t inv_size = divisor_high.size + 1;
                if (t_inv.size() < inv_size) t_inv.resize(inv_size);
                Span inv_span(t_inv.data(), inv_size);

                // OPT: divisor_high 足够大时用 fast 路径 (fftMulPre + DFT 复用, 4 FFT)
                //     否则用 slow 路径 (absMul, 6 FFT)
                if (divisor_high.size >= FFT_MUL_THRESHOLD)
                {
                    thread_local std::vector<double> inv_dft_buf_c1, div_dft_buf_c1;
                    size_t inv_fl = int_ceil2(2 * divisor_high.size + 1);
                    size_t div_fl = int_ceil2(2 * divisor_high.size);
                    if (inv_dft_buf_c1.size() < inv_fl) inv_dft_buf_c1.resize(inv_fl);
                    if (div_dft_buf_c1.size() < div_fl) div_dft_buf_c1.resize(div_fl);
                    prepareDFT(divisor_high, div_dft_buf_c1.data(), div_fl);
                    absInvNewton(divisor_high, inv_span, div_dft_buf_c1.data(), div_fl);
                    prepareDFT(inv_span, inv_dft_buf_c1.data(), inv_fl);
                    absDivNewtonWithInvFast(dividend_high, divisor_high, quotient, inv_span,
                                            inv_dft_buf_c1.data(), inv_fl,
                                            div_dft_buf_c1.data(), div_fl);
                }
                else
                {
                    absInvNewton(divisor_high, inv_span);
                    absDivNewtonWithInv(dividend_high, divisor_high, quotient, inv_span);
                }
            }
            
            thread_local std::vector<Limb> t_prod;
            size_t prod_size = quot_len + shift_len;
            if (t_prod.size() < prod_size) t_prod.resize(prod_size);
            Span prod_span(t_prod.data(), prod_size);
            View divisor_low(divisor.begin(), shift_len);
            absMul(divisor_low, quotient, prod_span);
            prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
            dividend.size = count_true_length(dividend.ptr, dividend.size);
            dividend_high.size = count_true_length(dividend_high.ptr, dividend_high.size);
            int count = 0;
            while (absCompare(prod_span, dividend) > 0)
            {
                assert(count < 2);
                count++;
                absSub1(quotient, 1, quotient); 
                quotient.size = count_true_length(quotient.ptr, quotient.size);
                if (absAdd(dividend_high, divisor_high, dividend_high)) 
                {
                    size_t add_len = std::max(dividend_high.size, divisor_high.size);
                    dividend_high[add_len] = 1;
                    dividend_high.size = add_len + 1;
                }
                else
                {
                    dividend_high.size = std::max(dividend_high.size, divisor_high.size);
                }
                dividend.size = dividend_high.size + shift_len;
                absSub(prod_span, divisor_low, prod_span); 
                prod_span.size = count_true_length(prod_span.ptr, prod_span.size);
            }
            absSub(dividend, prod_span, dividend);
            dividend.size = count_true_length(dividend.ptr, dividend.size);
            assert(absCompare(dividend, divisor) < 0);
        }

        // GMP mu_div_qr 风格分块除法: 用 in 位近似逆替代 len2+1 位精确逆
        // dividend: len1 位（原地修改为余数）
        // divisor: len2 位（已归一化，高位 >= HALF_BASE）
        // quotient: len1 - len2 位（输出，高位 1 位已预处理）
        // in: 近似逆精度（in <= len2）
        static void absDivMu(Span dividend, View divisor, Span quotient, size_t in)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            size_t len1 = dividend.size, len2 = divisor.size;
            assert(in <= len2);
            assert(in >= 1);
            assert(divisor[len2 - 1] >= HALF_BASE);

#ifdef PROFILE_DIV
            auto _mu_t0 = std::chrono::high_resolution_clock::now();
#endif

            // 1. 计算近似逆: divisor 高 in 位的精确逆
            // absInvNewton(divisor_high, inv) = floor(B^(2*in) / divisor_high), 输出 in+1 位
            // inv * divisor_high ≈ B^(2*in), inv ≈ B^(len2+in) / divisor
            thread_local std::vector<Limb> t_inv;
            size_t inv_size = in + 1;
            if (t_inv.size() < inv_size) t_inv.resize(inv_size);
            Span inv_span(t_inv.data(), inv_size);

            // 2. DFT 预计算
            // fftMulPre #1: divid_high(this_in+1) * inv(in+1), 卷积长度 <= 2*in+1
            // fftMulPre #2: qhat(this_in+1) * divisor(len2), 卷积长度 <= len2+in
            thread_local std::vector<double> inv_dft_buf, divisor_dft_buf;
            size_t inv_float_len = int_ceil2(2 * in + 1);
            // OPT: divisor_float_len 从 int_ceil2(len2+in+1) 改为 int_ceil2(len2+in)
            //   conv_len = qhat_span.size + len2 - 1 = (this_in+1) + len2 - 1 = this_in + len2 <= in + len2
            //   安全: int_ceil2(len2+in) >= conv_len
            size_t divisor_float_len = int_ceil2(len2 + in);
            if (inv_dft_buf.size() < inv_float_len) inv_dft_buf.resize(inv_float_len);
            if (divisor_dft_buf.size() < divisor_float_len) divisor_dft_buf.resize(divisor_float_len);
#ifndef DISABLE_2NXN_CYCLIC
            // 2NXN 循环卷积 (GMP mu_div_qr.c L288-303):
            // fftMulPre #2 用 cyclic mod (B^m-1), FFT 大小 m=int_ceil2(len2+1)
            // 1M/500k: m=131072 (vs 线性卷积 262144), FFT 减半
            // unwrap: prod_low = C - window_high (GMP 近似, 修正循环处理误差)
            thread_local std::vector<double> divisor_dft_mod_buf;
            size_t cyclic_m = int_ceil2(len2 + 1);
            if (divisor_dft_mod_buf.size() < cyclic_m) divisor_dft_mod_buf.resize(cyclic_m);
#endif

            // B-1 优化: 当 in == len2 时, divisor 切片 = 完整 divisor, 可复用 DFT 给 absInvNewton
            if (in == len2)
            {
                prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
#ifndef DISABLE_2NXN_CYCLIC
                prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
#endif
            }
            else
            {
                // GMP 风格 Newton 逆 (默认启用): 1M/500k 实测 17.68ms vs absInvNewton 18.88ms (-6.3%)
                // CYCLIC_MIN_K=4096 阈值保证; k<4096 时 absInvNewtonGMP 内部回退到 absInvNewton 逻辑
                absInvNewtonGMP(divisor + (len2 - in), inv_span);
#ifndef DISABLE_2NXN_CYCLIC
                prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
#else
                prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
#endif
            }
            prepareDFT(inv_span, inv_dft_buf.data(), inv_float_len);

#ifdef PROFILE_DIV
            auto _mu_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: absInvNewton+prepareDFT: %.3f ms (in=%zu, inv_fl=%zu, div_fl=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t1 - _mu_t0).count(), in, inv_float_len, divisor_float_len);
            auto _mu_t1b = std::chrono::high_resolution_clock::now();
#endif

            // 3. 分块循环（从高位向低位）
            // qn = quotient.size, 每个 block 处理 this_in = min(in, qn_remaining) 位商
            // 当 qn 不是 in 整数倍时, 最后一个 block 用较小 this_in (精度仍为 in+1, 安全)
            thread_local std::vector<Limb> tqhat, tprod;
            // OPT: 循环外预 resize 到最大 (qhat_len_max = in + in + 2, prod_len_max = len2 + in + 1)
            size_t qhat_len_max = 2 * in + 2;
            size_t prod_len_max = len2 + in + 1;
#ifndef DISABLE_2NXN_CYCLIC
            // fftMulModBm1Pre 需要 cyclic_m 长度缓冲, cyclic_m 可能 > prod_len_max
            prod_len_max = std::max(prod_len_max, cyclic_m);
#endif
            if (tqhat.size() < qhat_len_max) tqhat.resize(qhat_len_max);
            if (tprod.size() < prod_len_max) tprod.resize(prod_len_max);
            size_t qn = quotient.size;
            size_t qn_remaining = qn;
#ifdef PROFILE_DIV
            auto _mu_t2 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: gap1 (fprintf=%.3f ms, resize=%0.3f ms)\n",
                    std::chrono::duration<double, std::milli>(_mu_t1b - _mu_t1).count(),
                    std::chrono::duration<double, std::milli>(_mu_t2 - _mu_t1b).count());
#endif

            while (qn_remaining > 0)
            {
                size_t this_in = std::min(in, qn_remaining);

                // 当前块窗口: dividend[qn_remaining - this_in .. qn_remaining + len2 - 1]
                // 物理上 len1 = len2 + qn, 窗口长度 = len2 + this_in
                Span window(dividend.ptr + (qn_remaining - this_in), len2 + this_in);
                Span quot_block(quotient.ptr + (qn_remaining - this_in), this_in);

                // divid_high = window 高 this_in+1 位 (从 window[len2-1] 开始)
                Span divid_high = window + (len2 - 1);

                // qhat_full = divid_high * inv, 长度 = this_in + in + 2
                size_t qhat_len = divid_high.size + inv_span.size;
                Span qhat_full(tqhat.data(), qhat_len);
#ifdef PROFILE_DIV
                auto _blk_t0 = std::chrono::high_resolution_clock::now();
#endif
                fftMulPre(divid_high, inv_dft_buf.data(), inv_span.size, inv_float_len, qhat_full);
#ifdef PROFILE_DIV
                auto _blk_t1 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof]   block %zu: fftMulPre #1 (divid_high*inv, fl=%zu): %.3f ms\n",
                        (qn - qn_remaining) / in, inv_float_len,
                        std::chrono::duration<double, std::milli>(_blk_t1 - _blk_t0).count());
#endif

                // qhat = qhat_full 的高 this_in+1 位 (跳过低 in+1 位)
                // qhat ≈ divid_high * inv / B^(in+1) ≈ Q_block (本块真实商)
                Span qhat_span = qhat_full + (in + 1);

                // prod = divisor * qhat, 长度 = len2 + this_in + 1
                size_t prod_len = len2 + qhat_span.size;
                Span prod_span(tprod.data(), prod_len);
#ifdef PROFILE_DIV
                auto _blk_t2 = std::chrono::high_resolution_clock::now();
#endif
#ifndef DISABLE_2NXN_CYCLIC
                // 2NXN 循环卷积 (GMP mu_divappr_q.c L220-L232):
                // C = (qhat * divisor) mod (B^cyclic_m - 1), FFT 大小 cyclic_m (减半)
                // unwrap (GMP L226-L227): tp[0..wn-1] -= rp[dn-wn..dn-1] (rp 高 wn 位 ≈ prod 高 wn 位)
                // 修正 (GMP L228-L230): cx-cy 进位调整, 反映 cyclic 边界误差
                {
                    Span prod_mod_span(tprod.data(), cyclic_m);
                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);
                    // FIX: 当 len2+this_in <= cyclic_m 时, 卷积长度 <= cyclic_m, 循环卷积 == 线性卷积, 无需 unwrap
                    //   (原代码 size_t wn = len2+this_in-cyclic_m 在此情况下 underflow 成巨大数, 导致越界 SIGSEGV)
                    if (len2 + this_in > cyclic_m)
                    {
                        size_t wn = len2 + this_in - cyclic_m;
                        // GMP L226: tp[0..wn-1] -= rp[dn-wn..dn-1]
                        //   moptm window 布局: [np_new(低 this_in), rp_old(高 len2)]
                        //   rp_old = window[this_in .. len2+this_in-1]
                        //   rp_old[dn-wn..dn-1] = window[len2+this_in-wn .. len2+this_in-1]
                        Span prod_low_wn(prod_mod_span.ptr, wn);
                        View rp_high_wn(window.ptr + (len2 + this_in - wn), wn);
                        bool borrow = absSub(prod_low_wn, rp_high_wn, prod_low_wn);
                        // GMP L227: cy = mpn_sub_1 (tp + wn, tp + wn, tn - wn, cy)
                        Span prod_rest(prod_mod_span.ptr + wn, cyclic_m - wn);
                        if (borrow) absSub1(prod_rest, 1, prod_rest);
                        // GMP L228: cx = mpn_cmp (rp + dn - in, tp + dn, tn - dn) < 0
                        //   rp_old[dn-in..] = window[len2 .. len2+cmp_len-1], cmp_len = cyclic_m - len2
                        //   (cmp_len < this_in 因 wn > 0 ⟹ cyclic_m < len2+this_in)
                        size_t cmp_len = cyclic_m - len2;
                        View rp_cmp(window.ptr + len2, cmp_len);
                        View tp_cmp(tprod.data() + len2, cmp_len);
                        bool cx = (absCompare(rp_cmp, tp_cmp) < 0);
                        // GMP L229: ASSERT_ALWAYS (cx >= cy) — GMP 保证, moptm 可能违反
                        // GMP L230: mpn_incr_u (tp, cx - cy) — 用有符号运算正确处理 cx < borrow
                        int32_t incr_signed = int32_t(cx) - int32_t(borrow);
                        if (incr_signed > 0) {
                            absAdd1(prod_mod_span, 1, prod_mod_span);  // tp += 1
                        } else if (incr_signed < 0) {
                            absSub1(prod_mod_span, 1, prod_mod_span);  // tp -= 1
                        }
                    }
                    // 不重建 prod 高位! 真实 prod 高 wn 位由 r-based 修正循环处理
                }
#else
                fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);
#endif
#ifdef PROFILE_DIV
                auto _blk_t3 = std::chrono::high_resolution_clock::now();
#ifndef DISABLE_2NXN_CYCLIC
                PROF_PRINT("  [prof]   block %zu: fftMulModBm1Pre #2 (qhat*divisor mod B^%zu-1): %.3f ms\n",
                        (qn - qn_remaining) / in, cyclic_m,
                        std::chrono::duration<double, std::milli>(_blk_t3 - _blk_t2).count());
#else
                PROF_PRINT("  [prof]   block %zu: fftMulPre #2 (qhat*divisor, fl=%zu): %.3f ms\n",
                        (qn - qn_remaining) / in, divisor_float_len,
                        std::chrono::duration<double, std::milli>(_blk_t3 - _blk_t2).count());
#endif
#endif

                // === 简单双向修正 (qhat 可能偏大或偏小) ===
                // 先处理 qhat 偏大: 若 product > window, 则 product -= divisor, qhat -= 1
                // 再计算 remainder = window - product
                // 最后处理 qhat 偏小: 若 remainder >= divisor, 则 remainder -= divisor, qhat += 1
#ifdef PROFILE_DIV
                auto _blk_t4 = std::chrono::high_resolution_clock::now();
#endif
                size_t wnd_full_len = len2 + this_in;
                size_t prod_full_len;
#ifndef DISABLE_2NXN_CYCLIC
                prod_full_len = cyclic_m;
#else
                prod_full_len = prod_len;
#endif

#ifndef DISABLE_2NXN_CYCLIC
                // === cyclic 路径修正逻辑 (GMP mu_divappr_q.c L235-271, r 方法 + 双向修正) ===
                // GMP: r = rp[dn-in] - tp[dn], 反映 qhat 偏差
                //   映射: dn=len2, in=this_in, rp[dn-in]=window[len2], tp[dn]=tprod[len2]
                //   window=[np_new(this_in), rp_old(len2)] → rp_old[i]=window[this_in+i]
                //   rp[dn-in]=rp_old[len2-this_in]=window[len2]
                // GMP 保证 r >= 0 (inv 偏小 → qhat 偏小); moptm 的 absInvNewton 可能产生偏大 inv,
                //   导致 r < 0 (qhat 偏大), 需要双向修正
                int32_t r = int32_t(window[len2]) - int32_t(tprod[len2]);

                // 减法: tprod[0..len2-1] = window[0..len2-1] - tprod[0..len2-1] (GMP L239-248)
                //   window[0..this_in-1]=np_new, window[this_in..len2-1]=rp_old[0..len2-this_in-1]
                //   cy = 高位 borrow
                bool cy;
                {
                    View np_chunk(window.ptr, this_in);
                    Span tp_low(tprod.data(), this_in);
                    cy = absSub(np_chunk, tp_low, tp_low);
                    if (len2 != this_in) {
                        Span tp_high(tprod.data() + this_in, len2 - this_in);
                        View rp_low(window.ptr + this_in, len2 - this_in);
                        bool cy2 = absSub(rp_low, tp_high, tp_high);
                        bool cy3 = false;
                        if (cy) {
                            cy3 = absSub1(tp_high, 1, tp_high);
                        }
                        cy = cy2 || cy3;
                    }
                }

                // GMP L254: r -= cy
                r -= int32_t(cy);

                // 双向修正: r > 0 表示 qhat 偏小 (需 qhat+1), r < 0 表示 qhat 偏大 (需 qhat-1)
                // GMP L255-264: while (r != 0) { qhat++; rp -= divisor; r -= cy; }
                //   加 10 次限制防止 unwrap 误差导致的异常循环
                int corr_cnt = 0;
                while (r != 0 && corr_cnt < 10) {
                    if (r > 0) {
                        // qhat 偏小: qhat+1, rp -= divisor
                        absAdd1(qhat_span, 1, qhat_span);
                        bool b = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r -= int32_t(b);  // b=1 表示 rp < divisor (有借位), r 减小
                    } else {
                        // qhat 偏大: qhat-1, rp += divisor
                        absSub1(qhat_span, 1, qhat_span);
                        bool carry = absAdd(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r += int32_t(carry);  // carry=1 表示溢出 (rp 原为补码负数), r 增大
                    }
                    corr_cnt++;
                }

                // GMP L265-270: if (rp >= divisor) { qhat++; rp -= divisor; }
                if (absCompare(Span(tprod.data(), len2), divisor) >= 0) {
                    absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    absAdd1(qhat_span, 1, qhat_span);
                }
#else
                // --- 修正 1: product > window ? (qhat 偏大) ---
                int corr_down = 0;
                while (corr_down < 10) {
                    // 比较 product 和 window (只比较 window 长度的部分)
                    bool prod_gt = false;
                    // product 高位(超过 wnd_full_len 的部分)是否非零
                    if (prod_full_len > wnd_full_len) {
                        for (size_t i = wnd_full_len; i < prod_full_len; i++) {
                            if (tprod[i] != 0) { prod_gt = true; break; }
                        }
                    }
                    if (!prod_gt) {
                        int cmp = absCompare(View(tprod.data(), wnd_full_len), window);
                        prod_gt = (cmp > 0);
                    }
                    if (!prod_gt) break;
                    // product -= divisor (只减低 len2 位, 借位传播)
                    bool borrow = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    size_t bi = len2;
                    while (borrow && bi < prod_full_len) {
                        if (tprod[bi] > 0) {
                            tprod[bi]--;
                            borrow = false;
                        } else {
                            tprod[bi] = Limb(BASE - 1);
                            bi++;
                        }
                    }
                    absSub1(qhat_span, 1, qhat_span);
                    corr_down++;
                }

                // --- 计算 new_rp = window - product (低 len2 位) ---
                // 计算 borrow: product[0..wnd_full_len-1] <= window ?
                bool cy;
                {
                    // 先减低 this_in 位: tprod_low = np_new - tprod_low
                    View np_chunk(window.ptr, this_in);
                    Span tp_low(tprod.data(), this_in);
                    cy = absSub(np_chunk, tp_low, tp_low);
                    // 再减高 len2-this_in 位（带借位）: tprod_high = rp_low - tprod_high - cy
                    if (len2 != this_in) {
                        Span tp_high(tprod.data() + this_in, len2 - this_in);
                        View rp_low(window.ptr + this_in, len2 - this_in);
                        bool cy2 = absSub(rp_low, tp_high, tp_high);
                        bool cy3 = false;
                        if (cy) {
                            cy3 = absSub1(tp_high, 1, tp_high);
                        }
                        cy = cy2 || cy3;
                    }
                    // 高位借位( product[wnd_full_len..] > 0 则 cy = true, 已由修正 1 保证 product <= window )
                }

                // --- 修正 2: remainder >= divisor ? (qhat 偏小) ---
                int corr_up = 0;
                while (corr_up < 10) {
                    if (absCompare(Span(tprod.data(), len2), divisor) < 0) break;
                    absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++;
                }
#endif

                // --- 把新 rp 复制到 window ---
                std::copy(tprod.data(), tprod.data() + len2, window.ptr);
                window.size = len2;
#ifdef PROFILE_DIV
                auto _blk_t5 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof]   block %zu: correct+sub: %.3f ms\n",
                        (qn - qn_remaining) / in,
                        std::chrono::duration<double, std::milli>(_blk_t5 - _blk_t4).count());
#endif

                // 复制 qhat 到 quotient (去掉最高位 0)
                // 修正后 qhat = Q_block < B^this_in, 所以 qhat_span[this_in] = 0
                assert(qhat_span[qhat_span.size - 1] == 0);
                qhat_span.size--;
                std::copy(qhat_span.begin(), qhat_span.end(), quot_block.begin());

                qn_remaining -= this_in;
            }

#ifdef PROFILE_DIV
            auto _mu_t3 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: blocks loop: %.3f ms (qn=%zu, in=%zu, blocks=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t3 - _mu_t2).count(), qn, in, (qn + in - 1) / in);
            PROF_PRINT("  [prof] absDivMu: total: %.3f ms (len1=%zu, len2=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t3 - _mu_t0).count(), len1, len2);
#endif
            // 清零高位（仅低 len2 位是有效 remainder）
            if (len2 < dividend.size) {
                std::fill(dividend.ptr + len2, dividend.ptr + dividend.size, Limb(0));
            }
        }

        static void absDivNewtonCore2(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)
            {
                return;
            }
            assert(divisor.size > 0);
            size_t len1 = dividend.size, len2 = divisor.size;
            Limb divisor_high = divisor[len2 - 1];
            assert(divisor_high >= HALF_BASE);
#ifdef PROFILE_DIV
            auto _p_t0 = std::chrono::high_resolution_clock::now();
#endif
            thread_local std::vector<Limb> t_inv;
            size_t inv_size = len2 + 1;
            if (t_inv.size() < inv_size) t_inv.resize(inv_size);
            Span inv_span(t_inv.data(), inv_size);
            size_t blocks = len1 / len2, len1_rem = len2 * blocks;
            auto divid_it = dividend.ptr + (len1_rem - len2);
            auto quot_it = quotient.ptr + (len1_rem - len2);

             // 预计算 divisor_dft（同时用于 absInvNewton 的 DFT 复⽤和 fast blocks）
             thread_local std::vector<double> inv_dft_buf, divisor_dft_buf;
             size_t inv_float_len = int_ceil2(len2 * 2 + 1);
             size_t divisor_float_len = int_ceil2(len2 * 2);
             bool has_divisor_dft = false;
              // 方案 D: blocks >= 2 启用 fast path（原为 blocks >= 3）
              if (blocks >= 2) /* if (blocks >= 3) */
              {
                  if (divisor_dft_buf.size() < divisor_float_len)
                      divisor_dft_buf.resize(divisor_float_len);
                  prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                  has_divisor_dft = true;
              }
            if (has_divisor_dft)
            {
                absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
            }
            else
            {
                absInvNewton(divisor, inv_span);
            }
#ifdef PROFILE_DIV
            auto _p_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absInvNewton: %.3f ms (len2=%zu, dft_reuse=%d)\n",
                    std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count(), len2, has_divisor_dft);
#endif

            // 方案 D: blocks >= 2 启用 fast path（原为 blocks >= 3）
            if (blocks >= 2) /* if (blocks >= 3) */
            {
                if (inv_dft_buf.size() < inv_float_len)
                    inv_dft_buf.resize(inv_float_len);
                prepareDFT(inv_span, inv_dft_buf.data(), inv_float_len);
#ifdef PROFILE_DIV
                auto _p_t2 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof] prepareDFT: %.3f ms (inv_float_len=%zu, divisor_float_len=%zu)\n",
                        std::chrono::duration<double, std::milli>(_p_t2 - _p_t1).count(), inv_float_len, divisor_float_len);
                auto _p_t3 = std::chrono::high_resolution_clock::now();
#endif
                absDivNewtonWithInvFast(dividend + (len1_rem - len2), divisor, quotient + (len1_rem - len2), inv_span, inv_dft_buf.data(), inv_float_len, divisor_dft_buf.data(), divisor_float_len);
                while (divid_it > dividend.ptr)
                {
                    divid_it -= len2;
                    quot_it -= len2;
                    absDivNewtonWithInvFast(Span(divid_it, len2 * 2), divisor, Span(quot_it, len2), inv_span, inv_dft_buf.data(), inv_float_len, divisor_dft_buf.data(), divisor_float_len);
                }
#ifdef PROFILE_DIV
                auto _p_t4 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof] Core2 loop: %.3f ms (%zu blocks, %zu fftMulPre calls)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t3).count(), blocks, size_t(blocks * 2));
                PROF_PRINT("  [prof] Total Core2: %.3f ms (len1=%zu, len2=%zu)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t0).count(), len1, len2);
#endif
            }
            else
            {
#ifdef PROFILE_DIV
                auto _p_t3 = std::chrono::high_resolution_clock::now();
#endif
                absDivNewtonWithInv(dividend + (len1_rem - len2), divisor, quotient + (len1_rem - len2), inv_span);
                while (divid_it > dividend.ptr)
                {
                    divid_it -= len2;
                    quot_it -= len2;
                    absDivNewtonWithInv(Span(divid_it, len2 * 2), divisor, Span(quot_it, len2), inv_span);
                }
#ifdef PROFILE_DIV
                auto _p_t4 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof] Core1 loop (slow): %.3f ms (%zu blocks)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t3).count(), blocks);
                PROF_PRINT("  [prof] Total Core2: %.3f ms (len1=%zu, len2=%zu)\n",
                        std::chrono::duration<double, std::milli>(_p_t4 - _p_t0).count(), len1, len2);
#endif
            }
        }
            
        void absDivRem(const Integer &divisor, Integer &quotient, Integer &remainder) const
        {
            size_t len1 = this->length(), len2 = divisor.length();
            int cmp = absCompare(this->getView(), divisor.getView());
            if (cmp == 0)
            {
                quotient = Limb(1);
                remainder = Limb(0);
            }
            else if (cmp < 0)
            {
                quotient = Limb(0);
                remainder = *this;
            }
            else if (len2 == 1)
            {
                quotient = *this;
                remainder = quotient.selfDivRem1(divisor.data[0]);
            }
            else
            {
                
                Limb factor = divisorNormalizeFactor(divisor.getView());
                Integer dividend_norm, divisor_norm;
                if (factor == 1)
                {
                    dividend_norm = *this;
                    divisor_norm = divisor;
                }
                else
                {
                    dividend_norm = (*this) * factor;
                    divisor_norm = divisor * factor;
                }
                size_t len1 = dividend_norm.length(), len2 = divisor_norm.length();
                assert(len2 == divisor.length());
                size_t quot_len = len1 - len2 + 1;
                quotient.data.resize(quot_len);
                Span dividend_span = dividend_norm.getSpan(), divisor_span = divisor_norm.getSpan();
                Span high = dividend_span + (quot_len - 1); 
                if (absCompare(View(high), View(divisor_span)) >= 0)
                {
                    quotient.data[quot_len - 1] = 1;
                    absSub(high, divisor_span, high);
                }
                else
                {
                    quotient.data[quot_len - 1] = 0;
                }
                
                Span quot_span(quotient.data.data(), len1 - len2);
                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);
                }
                else if (len1 < len2 * 2)
                {
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);
                }
                else
                {
                    // GMP mu_div_qr 风格: 计算近似逆精度 in
                    size_t qn_mu = len1 - len2;
                    size_t mu_in;
                    if (qn_mu > len2)
                    {
                        mu_in = (qn_mu - 1) / ((qn_mu - 1) / len2 + 1) + 1;
                    }
                    else if (3 * qn_mu > len2)
                    {
                        // 自适应 mu_in 选择: 比较 2块 vs 4块 的 divisor FFT size
                        // profiling 发现: blocks loop FFT 与 in 有关 (非原注释所述"无关")
                        // 当 4块的 int_ceil2(len2+in4) < 2块的 int_ceil2(len2+in2) 时,
                        //   FFT size 减半的收益 > 块数翻倍的开销, 用 4块
                        // 否则用 2块, 避免无效增加块数
                        size_t in2 = (qn_mu - 1) / 2 + 1;
                        size_t in4 = (qn_mu - 1) / 4 + 1;
                        if (in2 > len2) in2 = len2;
                        if (in4 > len2) in4 = len2;
#ifdef DIV_MU_IN_QUARTER
                        mu_in = in4;  // 强制 4 块
#elif defined(DIV_MU_IN_HALF)
                        mu_in = in2;  // 强制 2 块 (旧版 default)
#else
                        size_t div_fl2 = int_ceil2(len2 + in2);
                        size_t div_fl4 = int_ceil2(len2 + in4);
                        mu_in = (div_fl4 < div_fl2) ? in4 : in2;
#endif
                    }
                    else
                    {
                        mu_in = qn_mu;
                    }
                    if (mu_in < len2)
                    {
                        absDivMu(dividend_span, divisor_span, quot_span, mu_in);
                    }
                    else
                    {
                        absDivNewtonCore2(dividend_span, divisor_span, quot_span);
                    }
                }
                dividend_norm.removeLeadingZero();
                
                if (factor != 1)
                {
                    Limb rem = dividend_norm.selfDivRem1(factor);
                    assert(rem == 0);
                }
                remainder = std::move(dividend_norm);
                remainder.removeLeadingZero();
                quotient.removeLeadingZero();
            }
        }
        static Limb divisorNormalizeFactor(View divisor)
        {
            assert(divisor.size > 0);
            constexpr int HALF_BASE_BITS = hint_bit_length<uint32_t>(HALF_BASE);
            Limb high_limb = divisor[divisor.size - 1];
            if (high_limb >= HALF_BASE)
            {
                return 1;
            }
            int bits = hint_bit_length<uint32_t>(high_limb);
            int shift = HALF_BASE_BITS - bits;
            Limb2 carry = 0;
            for (size_t i = 0; i < divisor.size - 1; i++)
            {
                carry += Limb2(divisor[i]) << shift;
                carry /= BASE;
            }
            carry += Limb2(high_limb) << shift;
            if (carry < HALF_BASE)
            {
                shift++;
            }
            else if (carry >= BASE)
            {
                shift--;
            }
            return Limb(1) << shift;
        }

        Integer &operator+=(const Integer &input)
        {
            return this->add(input.getView(), input.isNeg());
        }
        Integer &operator-=(const Integer &input)
        {
            return this->add(input.getView(), !input.isNeg());
        }
        Integer &operator*=(const Integer &input)
        {
            if (input.isZero() || this->isZero())
            {
                this->clear();
            }
            else
            {
                size_t len1 = this->length(), len2 = input.length();
                this->data.resize(len1 + len2);
                absMul(Span(this->data.data(), len1), input.getView(), this->getSpan());
            }
            this->setSign(this->isNeg() != input.isNeg());
            this->removeLeadingZero();
            return *this;
        }
        Integer &operator*=(Limb input)
        {
            if (input == 0)
            {
                this->clear();
            }
            else if (input > 1)
            {
                assert(input < BASE);
                Limb carry = absMul1(this->getSpan(), input, this->getSpan());
                if (carry > 0)
                {
                    this->data.push_back(carry);
                }
            }
            this->removeLeadingZero();
            return *this;
        }
        Integer &operator/=(const Integer &input)
        {
            Integer quotient, remainder;
            this->absDivRem(input, quotient, remainder);
            if (this->isNeg() == input.isNeg())
            {
                quotient.setSign(false);
            }
            else
            {
                if (!remainder.isZero())
                {
                    quotient += Limb(1);
                }
                quotient.setSign(true);
            }
            quotient.removeLeadingZero();
            *this = std::move(quotient);
            return *this;
        }
        Integer &operator%=(const Integer &input)
        {
            Integer quotient, remainder;
            this->absDivRem(input, quotient, remainder);
            remainder.setSign(input.isNeg());
            if ((!remainder.isZero()) && (this->isNeg() != input.isNeg()))
            {
                remainder = input - remainder;
            }
            remainder.removeLeadingZero();
            *this = std::move(remainder);
            return *this;
        }

        friend Integer operator+(Integer lhs, const Integer &rhs)
        {
            lhs += rhs;
            return lhs;
        }
        friend Integer operator-(Integer lhs, const Integer &rhs)
        {
            lhs -= rhs;
            return lhs;
        }
        friend Integer operator*(Integer lhs, const Integer &rhs)
        {
            lhs *= rhs;
            return lhs;
        }
        friend Integer operator/(Integer lhs, const Integer &rhs)
        {
            lhs /= rhs;
            return lhs;
        }
        friend Integer operator%(Integer lhs, const Integer &rhs)
        {
            lhs %= rhs;
            return lhs;
        }

        friend Integer operator*(Integer lhs, Limb rhs)
        {
            if (rhs == 0)
            {
                return Integer(0);
            }
            if (rhs > 1)
            {
                assert(rhs < BASE);
                Span lhs_span = lhs.getSpan();
                Limb carry = absMul1(lhs_span, rhs, lhs_span);
                if (carry != 0)
                {
                    lhs.data.push_back(carry);
                }
            }
            lhs.removeLeadingZero();
            return lhs;
        }

    private:
        DataVec data;
        bool sign;
    };

    // Force emission of global symbols for static member functions
    // that would otherwise be inlined-only in this TU.
    [[gnu::unused]] volatile auto _hint_export_absMul  = Integer::absMul;
    [[gnu::unused]] volatile auto _hint_export_absSub  = Integer::absSub;
    [[gnu::unused]] volatile auto _hint_export_absAdd  = Integer::absAdd;
    [[gnu::unused]] volatile auto _hint_export_absSub1 = Integer::absSub1;
    [[gnu::unused]] volatile auto _hint_export_absAdd1 = Integer::absAdd1;
    [[gnu::unused]] volatile auto _hint_export_absCompare = Integer::absCompare;
    [[gnu::unused]] volatile auto _hint_export_absSqr = Integer::absSqr;

    static const Integer fib_table[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89,
                                        144, 233, 377, 610, 987, 1597, 2584, 4181, 6765};
    constexpr size_t table_size = sizeof(fib_table) / sizeof(Integer);
    
    void fib1(size_t n, Integer &fib_n, Integer &fib_n1)
    {
        if (n < table_size - 1)
        {
            fib_n = fib_table[n];
            fib_n1 = fib_table[n + 1];
            return;
        }
        Integer fib_m, fib_m_p1;
        
        
        
        
        
        
        
        size_t m = n / 2;
        fib1(m, fib_m, fib_m_p1);
        fib_m_p1 *= fib_m; 
        fib_m.square();    
        fib_n = fib_m_p1 + fib_m_p1 - fib_m;
        fib_n1 = fib_m_p1 + fib_m + fib_m;
        fib_n1 += Integer(m % 2 == 0 ? 1 : -1);
        if (n % 2 == 1)
        {
            std::swap(fib_n, fib_n1);
            fib_n1 += fib_n;
        }
    }
    Integer fib1(size_t n)
    {
        if (n < table_size)
        {
            return fib_table[n];
        }
        Integer fib_m, fib_m_p1;
        fib1(n / 2, fib_m, fib_m_p1);
        if (n % 2 == 0)
        {
            fib_m_p1 += fib_m_p1;
            return (fib_m_p1 -= fib_m) * fib_m;
        }
        return fib_m.square() + fib_m_p1.square();
    }

    
    void fib2(size_t n, Integer &fib_n, Integer &fib_n_m1)
    {
        if (n < table_size)
        {
            fib_n = fib_table[n];
            fib_n_m1 = fib_table[n - 1];
            return;
        }
        Integer fib_m, fib_m_m1;
        size_t m = n / 2;
        fib2(m, fib_m, fib_m_m1);
        fib_m_m1.square(); 
        fib_m.square();    
        
        
        fib_n = fib_m * 4 - fib_m_m1;
        fib_n += Integer(m % 2 == 0 ? 2 : -2);
        fib_n_m1 = fib_m_m1 + fib_m;
        if (n % 2 == 1)
        {
            fib_n_m1 = fib_n - fib_n_m1;
        }
        else
        {
            fib_n -= fib_n_m1;
        }
    }
    Integer fib2(size_t n)
    {
        if (n < table_size)
        {
            return fib_table[n];
        }
        Integer fib_m, fib_m_m1;
        fib2(n / 2, fib_m, fib_m_m1);
        if (n % 2 == 0)
        {
            return fib_m * (fib_m + fib_m_m1 * 2);
        }
        fib_m *= 2;
        return (fib_m + fib_m_m1) * (fib_m - fib_m_m1) + Integer((n / 2) % 2 == 0 ? 2 : -2);
    }
}

#endif

// === 快速输出 (oBuffer 零拷贝, 查表替代除法) ===
#include <cstdio>
#include <cstring>
#ifdef __linux__
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace {
#if defined(HINT_OP_DIV)
    static char oBuffer[32 << 20], *oCursor = oBuffer;  // 32MB (DIV T≤2×10⁶ 无总字符数限制)
#else
    static char oBuffer[8 << 20], *oCursor = oBuffer;   // 8MB (ADD/MUL 总输出 ≤4MB)
#endif

    void writeHint(const hint::Integer& val) {
        oCursor += val.writeTo(oCursor);
    }

    void flushOutput() {
        std::fwrite(oBuffer, 1, oCursor - oBuffer, stdout);
    }

    // === 快速输入 (mmap 零拷贝 Linux / fread 一次性 Windows fallback) ===
    // 对齐 best/add.cpp 策略: SWAR 64-bit 批量找分隔符, 绕过 cin 流提取
    static char iBuffer[8 << 20];  // 8MB (LC 总输入 ≤ 4MB)
    static const char *iCursor = iBuffer, *iEnd = iBuffer;

    static void initInput() {
#ifdef __linux__
        struct stat status;
        if (fstat(STDIN_FILENO, &status) == 0 && status.st_size > 0) {
            void *p = mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0);
            if (p != MAP_FAILED) {
                iCursor = reinterpret_cast<const char *>(p);
                iEnd = iCursor + status.st_size;
                return;
            }
        }
#endif
        size_t n = std::fread(iBuffer, 1, sizeof(iBuffer) - 1, stdin);
        iBuffer[n] = '\n';  // 哨兵, 确保 SWAR 能终止
        iCursor = iBuffer;
        iEnd = iBuffer + n;
    }

    // SWAR 64-bit: 找从 p 起第一个 < 0x21 的字节 (空白/换行/制表), 返回 token 长度
    // 对齐 best/add.cpp readFromCursor 的位运算技巧
    // 边界安全: SWAR 仅在剩余 >=8 字节时运行; 末尾 0-7 字节走标量回退,
    //           避免 mmap 末页 (文件大小恰为页整数倍且无尾换行) 越界读
    static inline size_t swarTokenLen(const char *p) {
        size_t offset = 0;
        while (p + offset + sizeof(uint64_t) <= iEnd) {
            uint64_t data;
            std::memcpy(&data, p + offset, sizeof(data));
            uint64_t mask = (~data) & (data - 0x2121212121212121ULL) & 0x8080808080808080ULL;
            if (mask) {
                return offset + size_t(__builtin_ctzll(mask)) / 8;
            }
            offset += 8;
        }
        // 标量回退: 末尾 0-7 字节
        while (p + offset < iEnd && p[offset] >= 0x21) {
            offset++;
        }
        return offset;
    }

    // 从 iCursor 解析一个 Integer (支持负号), 推进 iCursor 到下一个 token
    static inline void parseInteger(hint::Integer &out) {
        const char *start = iCursor;
        size_t len = swarTokenLen(iCursor);
        out.fromCharRange(start, start + len);
        iCursor += len;
        // 跳过单个分隔符 (空格/换行)
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    }
}
#if defined(HINT_OP_ADD)
int main() {
#ifdef PROFILE_DIV
    clock_t _c0 = clock();
    PROF_PRINT("[ADD] static_init(from clock)=%.2fms\n", double(_c0) * 1000.0 / CLOCKS_PER_SEC);
#endif
    initInput();
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;

    hint::Integer a, b;
#ifdef BENCH_INTERNAL
    // 内部计时模式: 读1对, 循环计算 N 次, 每次单独计时
    parseInteger(a);
    parseInteger(b);
    // warmup
    hint::Integer c;
    for (int i = 0; i < 3; i++) { c = a; c += b; }
    const int N = 20;
    double times[20];
    double total = 0;
    for (int i = 0; i < N; i++) {
        c = a;
        auto t0 = std::chrono::high_resolution_clock::now();
        c += b;
        auto t1 = std::chrono::high_resolution_clock::now();
        times[i] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        total += times[i];
    }
    std::sort(times, times + N);
    fprintf(stderr, "MIN: %.3f  MED: %.3f  AVG: %.3f  MAX: %.3f  TOTAL: %.3f\n",
            times[0], times[N/2], total/N, times[N-1], total);
    writeHint(c);
    *oCursor++ = '\n';
    flushOutput();
    return 0;
#endif
#ifdef PROFILE_DIV
    double t_parse = 0, t_add = 0, t_write = 0;
    auto _t0 = std::chrono::high_resolution_clock::now();
#endif
    while (t--) {
#ifdef PROFILE_DIV
        auto _p0 = std::chrono::high_resolution_clock::now();
#endif
        parseInteger(a);
        parseInteger(b);
#ifdef PROFILE_DIV
        auto _p1 = std::chrono::high_resolution_clock::now();
        t_parse += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        a += b;
#ifdef PROFILE_DIV
        auto _p2 = std::chrono::high_resolution_clock::now();
        t_add += std::chrono::duration<double, std::milli>(_p2 - _p1).count();
#endif
        writeHint(a);
        *oCursor++ = '\n';
#ifdef PROFILE_DIV
        auto _p3 = std::chrono::high_resolution_clock::now();
        t_write += std::chrono::duration<double, std::milli>(_p3 - _p2).count();
#endif
    }
#ifdef PROFILE_DIV
    auto _t1 = std::chrono::high_resolution_clock::now();
#endif
    flushOutput();
#ifdef PROFILE_DIV
    auto _t2 = std::chrono::high_resolution_clock::now();
    double t_loop = std::chrono::duration<double, std::milli>(_t1 - _t0).count();
    double t_flush = std::chrono::duration<double, std::milli>(_t2 - _t1).count();
    FILE *_pf = std::fopen("prof_result.txt", "w");
    fprintf(_pf, "[ADD profile] parse=%.2fms add=%.2fms write=%.2fms loop=%.2fms flush=%.2fms main=%.2fms\n",
            t_parse, t_add, t_write, t_loop, t_flush, t_loop + t_flush);
    std::fclose(_pf);
#endif
    return 0;
}
#elif defined(HINT_OP_MUL)
int main() {
    initInput();
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    hint::Integer a, b;
#ifdef BENCH_INTERNAL
    // 内部计时模式: 读1对, 循环计算 N 次, 每次单独计时
    parseInteger(a);
    parseInteger(b);
    // warmup
    hint::Integer c;
    for (int i = 0; i < 3; i++) { c = a; c *= b; }
    const int N = 20;
    double times[20];
    double total = 0;
    for (int i = 0; i < N; i++) {
        c = a;
        auto t0 = std::chrono::high_resolution_clock::now();
        c *= b;
        auto t1 = std::chrono::high_resolution_clock::now();
        times[i] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        total += times[i];
    }
    std::sort(times, times + N);
    fprintf(stderr, "MIN: %.3f  MED: %.3f  AVG: %.3f  MAX: %.3f  TOTAL: %.3f\n",
            times[0], times[N/2], total/N, times[N-1], total);
    writeHint(c);
    *oCursor++ = '\n';
    flushOutput();
    return 0;
#endif
    while (t--) {
        parseInteger(a);
        parseInteger(b);
        a *= b;
        writeHint(a);
        *oCursor++ = '\n';
    }
    flushOutput();
    return 0;
}
#elif defined(HINT_OP_DIV)
int main() {
#ifdef PROFILE_DIV
    setvbuf(stderr, NULL, _IONBF, 0);
#endif
    initInput();
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    hint::Integer a, b, q, r;
    while (t--) {
        parseInteger(a);
        parseInteger(b);
        a.absDivRem(b, q, r);
        writeHint(q);
        *oCursor++ = ' ';
        writeHint(r);
        *oCursor++ = '\n';
    }
    flushOutput();
    return 0;
}
#elif defined(HINT_OP_TESTMOD)
// 测试 fftMulModBm1 正确性: 输入 t 组 (a, b, m), 对比 R1=fftMulModBm1 vs R2=线性卷积折叠
int main() {
    using namespace hint;
    using Limb = Integer::Limb;
    using Span = Integer::Span;
    int t;
    if (scanf("%d", &t) != 1) return 1;
    int pass_cnt = 0, fail_cnt = 0;
    while (t--) {
        static char buf_a[1 << 21], buf_b[1 << 21];
        size_t m;
        if (scanf("%s %s %zu", buf_a, buf_b, &m) != 3) return 1;
        // 修复: buf_a 是 char[N], 模板构造函数 Integer(const T&) [T=char[N]] 优先于
        // Integer(const char*), 导致 sign=input<0 指针比较错误. 用 const char* 中转.
        const char *sa = buf_a, *sb = buf_b;
        Integer a(sa), b(sb);
        size_t pa = a.length(), pb = b.length();

        // R1 = fftMulModBm1(a, b, m)
        std::vector<Limb> r1(m, 0);
        Span r1_span(r1.data(), m);
        Integer::fftMulModBm1(a.getView(), b.getView(), m, r1_span);

        // P = a * b (full convolution)
        std::vector<Limb> pbuf(pa + pb, 0);
        Span p_span(pbuf.data(), pa + pb);
        Integer::absMul(a.getView(), b.getView(), p_span);
        size_t plen = count_true_length(pbuf.data(), pa + pb);

        // R2 = fold P to m limbs + cyclic carry propagation
        std::vector<Limb> r2(m, 0);
        uint64_t carry = 0;
        for (size_t i = 0; i < m; i++) {
            uint64_t s = carry + (i < plen ? uint64_t(pbuf[i]) : 0);
            if (i + m < plen) s += pbuf[i + m];
            uint64_t q = s / Integer::BASE;
            r2[i] = Limb(s - q * Integer::BASE);
            carry = q;
        }
        while (carry > 0) {
            bool wrapped = true;
            for (size_t j = 0; j < m && carry > 0; j++) {
                uint64_t s = uint64_t(r2[j]) + carry;
                uint64_t q = s / Integer::BASE;
                r2[j] = Limb(s - q * Integer::BASE);
                carry = q;
                if (carry == 0) { wrapped = false; break; }
            }
            if (wrapped && carry == 1) {
                bool allzero = true;
                for (size_t j = 0; j < m; j++) if (r2[j]) { allzero = false; break; }
                if (allzero) { carry = 0; }
            }
        }
        size_t r2_len = count_true_length(r2.data(), m);
        size_t r1_len = count_true_length(r1.data(), m);

        bool pass = (r1_len == r2_len);
        if (pass) {
            for (size_t i = 0; i < r1_len; i++) {
                if (r1[i] != r2[i]) { pass = false; break; }
            }
        }
        if (pass) {
            pass_cnt++;
            printf("PASS m=%zu pa=%zu pb=%zu plen=%zu\n", m, pa, pb, plen);
        } else {
            fail_cnt++;
            printf("FAIL m=%zu pa=%zu pb=%zu plen=%zu r1_len=%zu r2_len=%zu\n",
                   m, pa, pb, plen, r1_len, r2_len);
            printf("R1:"); for (size_t i = 0; i < r1_len && i < 10; i++) printf(" %u", r1[i]); printf("\n");
            printf("R2:"); for (size_t i = 0; i < r2_len && i < 10; i++) printf(" %u", r2[i]); printf("\n");
        }
    }
    printf("=== %d PASS, %d FAIL ===\n", pass_cnt, fail_cnt);
    return 0;
}
#elif defined(HINT_OP_TESTNEWTON)
// 测试 absInvNewtonGMP vs absInvNewton 正确性 (fallback 模式下应完全一致)
// 输入: 第一行 t (组数), 然后每组 2 行 (a 和 b 的十进制字符串)
// a 仅用于标记测试规模, 实际只用 b 作为 divisor 计算逆
// 输出: 每组 PASS/FAIL, 末尾汇总
int main() {
    using namespace hint;
    using Limb = Integer::Limb;
    using Span = Integer::Span;
    using View = Integer::View;

    int t;
    if (scanf("%d", &t) != 1) return 1;
    int pass_cnt = 0, fail_cnt = 0;
    while (t--) {
        static char buf_a[1 << 21], buf_b[1 << 21];
        if (scanf("%s %s", buf_a, buf_b) != 2) return 1;
        // buf_a/buf_b 是 char[N], 模板构造函数 Integer(const T&) 会优先匹配,
        // 导致 sign=input<0 指针比较错误. 用 const char* 中转 (对齐 TESTMOD 修复)
        const char *sa = buf_a, *sb = buf_b;
        Integer a_int(sa), b_int(sb);
        size_t k = b_int.length();
        if (k == 0) {
            printf("SKIP (b=0)\n");
            continue;
        }

        // 分配两个独立 inv 缓冲区 (长度 k+1, 与 absInvNewton 约定一致)
        std::vector<Limb> inv1_buf(k + 1, 0), inv2_buf(k + 1, 0);
        Span inv1_span(inv1_buf.data(), k + 1);
        Span inv2_span(inv2_buf.data(), k + 1);
        View bv = b_int.getView();

        // 分别调用 absInvNewton (基线) 和 absInvNewtonGMP (待测, 当前 fallback)
        Integer::absInvNewton(bv, inv1_span);
        Integer::absInvNewtonGMP(bv, inv2_span);

        // 对比结果 (逐 limb 比较, 用 count_true_length 去前导零)
        size_t len1 = count_true_length(inv1_buf.data(), k + 1);
        size_t len2 = count_true_length(inv2_buf.data(), k + 1);
        bool pass = (len1 == len2);
        if (pass) {
            for (size_t i = 0; i < len1; i++) {
                if (inv1_buf[i] != inv2_buf[i]) { pass = false; break; }
            }
        }
        if (pass) {
            pass_cnt++;
            printf("PASS k=%zu inv_len=%zu (a_digits=%zu b_digits=%zu)\n",
                   k, len1, a_int.lengthBase10(), b_int.lengthBase10());
        } else {
            fail_cnt++;
            printf("FAIL k=%zu len1=%zu len2=%zu (a_digits=%zu b_digits=%zu)\n",
                   k, len1, len2, a_int.lengthBase10(), b_int.lengthBase10());
            printf("INV1 lo:"); for (size_t i = 0; i < len1 && i < 10; i++) printf(" %u", inv1_buf[i]); printf("\n");
            printf("INV2 lo:"); for (size_t i = 0; i < len2 && i < 10; i++) printf(" %u", inv2_buf[i]); printf("\n");
            printf("INV1 hi:"); for (size_t i = len1 > 10 ? len1 - 10 : 0; i < len1; i++) printf(" %u", inv1_buf[i]); printf("\n");
            printf("INV2 hi:"); for (size_t i = len2 > 10 ? len2 - 10 : 0; i < len2; i++) printf(" %u", inv2_buf[i]); printf("\n");
            // 找到第一个不同的 limb
            size_t max_len = std::max(len1, len2);
            for (size_t i = 0; i < max_len; i++) {
                Limb v1 = i < len1 ? inv1_buf[i] : 0;
                Limb v2 = i < len2 ? inv2_buf[i] : 0;
                if (v1 != v2) {
                    printf("DIFF @%zu: inv1=%u inv2=%u\n", i, v1, v2);
                    break;
                }
            }
        }
    }
    printf("=== %d PASS, %d FAIL ===\n", pass_cnt, fail_cnt);
    return 0;
}
#else
#error "Must define HINT_OP_ADD, HINT_OP_MUL, HINT_OP_DIV, HINT_OP_TESTMOD, or HINT_OP_TESTNEWTON"
#endif
