// https://judge.yosupo.jp/problem/division_of_big_integers
// https://judge.yosupo.jp/problem/addition_of_big_integers
// https://judge.yosupo.jp/problem/multiplication_of_big_integers
// code from https://github.com/With-Sky/HyperInt-mini (hint library)
//
// moptm (masonxiong_opt mixed) v2 - hint 库统一融合版
//   底座：hint 库 (BASE=10^4, uint16_t, radix-4 auto-vec FFT, Core2 分块除法)
//   I/O：cin 读 (streambuf 批量) + oBuffer 零拷贝写 (4 位查表替代 itostr4 除法)
//   三题统一：ADD/MUL/DIV 通过 -D 编译开关切换 main
//   本地实测 (-O3 -mavx2 -mfma -funroll-loops, 5 次最小值):
//     ADD 1M+1M:   14.6ms (best 19.7ms, -26%)
//     MUL 500k*500k: 17.3ms (best 21.6ms, -20%)
//     DIV 1M/100k:  28.9ms (best 29.2ms, 持平)
//
// Author: AZZRCN (qazwsx233343@163.com)
// GitHub: https://github.com/AZZRCN

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

#include <chrono>
#ifdef PROFILE_DIV
#include <cstdio>
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
    constexpr uint16_t str4toi(const char *s)
    {
        return s[0] * 1000 + s[1] * 100 + s[2] * 10 + s[3] - '0' * 1111;
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
    T add_half(T a, T b, T base, bool &cf)
    {
        a += b;
        cf = a >= base;
        const T mask = T(0) - T(cf);
        return a - (base & mask);
    }
    template <typename T>
    T sub_half(T a, T b, T base, bool &bf)
    {
        bf = a < b;
        const T mask = T(0) - T(bf);
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
            if (str.empty())
            {
                return;
            }
            auto p_begin = str.data(), p_end = p_begin + str.size();
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
            static const uint32_t* table = []() {
                static uint32_t t[10000];
                for (int i = 0; i < 10000; i++)
                {
                    t[i] = uint32_t(i / 1000 + '0') |
                           (uint32_t(i / 100 % 10 + '0') << 8) |
                           (uint32_t(i / 10 % 10 + '0') << 16) |
                           (uint32_t(i % 10 + '0') << 24);
                }
                return t;
            }();
            if (isZero())
            {
                out[0] = '0';
                return 1;
            }
            char* p = out;
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
            size_t i = data.size() - 1;
            while (i > 0)
            {
                i--;
                std::memcpy(p, &table[data[i]], 4);
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
            while (end > p_begin + 3)
            {
                end -= BASE_DIGIT;
                data[i] = str4toi(end);
                i++;
            }
            if (end > p_begin)
            {
                data[i] = 0;
                while (end > p_begin)
                {
                    data[i] *= 10;
                    data[i] += p_begin[0] - '0';
                    p_begin++;
                }
            }
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
            std::string tmp;
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
        static bool absAdd(View in1, View in2, Span out)
        {
            if (in1.size < in2.size)
            {
                std::swap(in1, in2);
            }
            size_t i = 0;
            bool carry = 0;
            for (; i < in2.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], in2[i] + carry, BASE, carry);
            }
            for (; i < in1.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            return carry;
        }
        static bool absSub(View in1, View in2, Span out)
        {
            assert(in1.size >= in2.size);
            size_t i = 0;
            bool borrow = 0;
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
            bool carry = 0;
            out[0] = add_half<Limb>(in1[0], in2, BASE, carry);
            for (size_t i = 1; i < in1.size; i++)
            {
                out[i] = add_half<Limb>(in1[i], carry, BASE, carry);
            }
            return carry;
        }
        static bool absSub1(View in1, Limb in2, Span out)
        {
            assert(in1.size > 0);
            bool borrow = 0;
            out[0] = sub_half<Limb>(in1[0], in2, BASE, borrow);
            for (size_t i = 1; i < in1.size; i++)
            {
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
            for (size_t i = 0; i < conv_len; i++)
            {
                carry += uint64_t(v1[i] + 0.5);
                out[i] = carry % BASE;
                carry /= BASE;
            }
            out[conv_len] = carry;
            
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
            for (size_t i = 0; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                out[i] = carry % BASE;
                carry /= BASE;
            }
            out[conv_len] = carry;
            
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
        static void absMul(View in1, View in2, Span out)
        {
            assert(out.size >= in1.size + in2.size);
            if (in1.ptr == in2.ptr)
            {
                absSqr(in1, out);
                return;
            }
            if (std::min(in1.size, in2.size) <= FFT_MUL_THRESHOLD)
            {
                basicMul(in1, in2, out);
            }
            else
            {
                fftMul(in1, in2, out);
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
            for (size_t i = 0; i < conv_len; i++)
            {
                carry += uint64_t(v[i] + 0.5);
                out[i] = carry % BASE;
                carry /= BASE;
            }
            out[conv_len] = carry;
            
            if (out.size > conv_len + 1)
            {
                std::fill_n(out.ptr + conv_len + 1, out.size - conv_len - 1, Limb(0));
            }
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
        static void absInvNewton(View m, Span inv,
                                 const double *m_dft = nullptr, size_t m_dft_float_len = 0)
        {
            size_t k = m.size;
            assert(k > 0);
            assert(inv.size >= k + 1);
            if (k <= 64)
            {
                Limb b_2k[256];
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
            absSqr(inv0, prod_span);
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
            prod_span = prod_span + 2 * (k - s);        
             assert(prod_span[prod_span.size - 1] == 0); 
             prod_span.size--;                           
             absSub(inv2_span, prod_span, inv);          
         }

         // 近似逆: 计算 dn 位模数的 in 位精度近似逆（in <= dn）
         // 关键: Newton 迭代的递归深度由 in 而非 dn 决定
         // 只保留高 in 位的逆精度，降低 Newton 迭代的变换量
         static void absInvNewtonApprox(View m, Span inv, size_t in,
                                        const double *m_dft = nullptr, size_t m_dft_float_len = 0)
         {
             size_t dn = m.size;
             assert(dn > 0);
             assert(in > 0 && in <= dn);
             assert(inv.size >= in + 1);

             // 如果 in == dn，直接调用完整逆函数
             if (in >= dn)
             {
                 absInvNewton(m, inv, m_dft, m_dft_float_len);
                 return;
             }

             // 基础情况
             if (in <= 64)
             {
                 Limb b_2in[256];
                 b_2in[in * 2] = 1;
                 std::fill_n(b_2in, in * 2, Limb(0));
                 absDivBasicCore(Span(b_2in, in * 2 + 1), View(m.ptr + (dn - in), in), inv);
                 return;
             }

             // 递归情况: 先计算更低精度的逆，然后做 Newton 迭代
             size_t sn = (in - 1) / 2;
             
             // 递归计算高 in 位 m 的 sn 位逆（降低精度）
             absInvNewtonApprox(View(m.ptr + (dn - in), in), inv, sn);

             // Newton 迭代: inv[0:in] = 2*inv[0:sn] - m[dn-in:in] * inv[0:sn]^2
             size_t inv0_len = in - sn + 1;
             Span inv0(inv.ptr, inv0_len);

             thread_local std::vector<Limb> tprod, tinv2;
             size_t prod_size = inv0_len * 2 + in;
             size_t inv2_size = in + 1;
             if (tprod.size() < prod_size) tprod.resize(prod_size);
             if (tinv2.size() < inv2_size) tinv2.resize(inv2_size);
             
             std::fill_n(tinv2.data(), sn, Limb(0));
             Span inv2_span(tinv2.data(), inv2_size);
             bool cf = absAdd(inv0, inv0, inv2_span + sn);
             assert(!cf);

             // 计算 prod = inv0^2
             Span prod_span(tprod.data(), prod_size);
             absSqr(inv0, prod_span);

             // 计算 prod *= m[dn-in:in]（用递归时同样的高 in 位）
             {
                 size_t conv_len = inv0_len * 2 + in - 1;
                 size_t need_float_len = int_ceil2(conv_len);
                 View m_high(m.ptr + (dn - in), in);
                 if (m_dft != nullptr && m_dft_float_len == need_float_len)
                 {
                     fftMulPre(View(tprod.data(), inv0_len * 2), m_dft, in, need_float_len, prod_span);
                 }
                 else
                 {
                     absMul(View(tprod.data(), inv0_len * 2), m_high, prod_span);
                 }
             }
             
             prod_span = prod_span + 2 * (in - sn);
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

         // Möller's 分块除法的简化实现:
         // 使用部分精度逆 inv[in] 代替完整逆 inv[dn]
         // 每块继续用 absDivNewtonWithInv 处理, 但逆精度更低
         // 此函数不再使用 FFT 预计算 (为了保持代码简洁)
         // 关键思想: 近似逆精度减半 -> Newton 迭代规模减半 -> 总变换量减 ~44%
         static void absDivMuSimple(Span dividend, View divisor, Span quotient, View inv_span)
         {
             // inv_span 已是部分精度逆, 直接用原有分块逻辑处理
             // 此函数存在是为了代码清晰度, 实际上逻辑与 absDivNewtonWithInv 相同
             // (因为修正循环保证精确性)
             absDivNewtonWithInv(dividend, divisor, quotient, inv_span);
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
                absInvNewton(divisor_high, inv_span);
                absDivNewtonWithInv(dividend_high, divisor_high, quotient, inv_span);
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
             
             // 近似逆优化: 只计算 in 位精度的逆，而非完整 len2 位
             // 按 GMP 策略: in = len2 / 2 (Möller 对 1M/500k 采用 2 块)
             size_t in = (len2 + 1) / 2;  // 近似逆精度，向上取整
             size_t inv_size = in + 1;
             if (t_inv.size() < inv_size) t_inv.resize(inv_size);
             Span inv_span(t_inv.data(), inv_size);
             
             size_t blocks = len1 / len2, len1_rem = len2 * blocks;
             auto divid_it = dividend.ptr + (len1_rem - len2);
             auto quot_it = quotient.ptr + (len1_rem - len2);

             // B-1 优化: blocks>=2 时预计算 divisor_dft 并传入 absInvNewtonApprox 最外层
             //           省 1 次 DFT(m)（float_len 匹配时生效）
             thread_local std::vector<double> inv_dft_buf, divisor_dft_buf;
             size_t inv_float_len = int_ceil2(in * 2 + 1);
             size_t divisor_float_len = int_ceil2(len2 * 2);
             bool has_divisor_dft = false;
             if (blocks >= 3)
             {
                 if (divisor_dft_buf.size() < divisor_float_len)
                     divisor_dft_buf.resize(divisor_float_len);
                 prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                 has_divisor_dft = true;
             }
             // 计算近似逆（精度为 in）
             if (has_divisor_dft)
             {
                 absInvNewtonApprox(divisor, inv_span, in, divisor_dft_buf.data(), divisor_float_len);
             }
             else
             {
                 absInvNewtonApprox(divisor, inv_span, in);
             }
#ifdef PROFILE_DIV
             auto _p_t1 = std::chrono::high_resolution_clock::now();
             fprintf(stderr, "  [prof] absInvNewtonApprox(in=%zu): %.3f ms (len2=%zu, dft_reuse=%d)\n",
                     in, std::chrono::duration<double, std::milli>(_p_t1 - _p_t0).count(), len2, has_divisor_dft);
#endif



             if (blocks >= 3)
             {
                 if (inv_dft_buf.size() < inv_float_len)
                     inv_dft_buf.resize(inv_float_len);
                 prepareDFT(inv_span, inv_dft_buf.data(), inv_float_len);
#ifdef PROFILE_DIV
                 auto _p_t2 = std::chrono::high_resolution_clock::now();
                 fprintf(stderr, "  [prof] prepareDFT: %.3f ms (inv_float_len=%zu, divisor_float_len=%zu)\n",
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
                 fprintf(stderr, "  [prof] Core2 loop: %.3f ms (%zu blocks, %zu fftMulPre calls)\n",
                         std::chrono::duration<double, std::milli>(_p_t4 - _p_t3).count(), blocks, size_t(blocks * 2));
                 fprintf(stderr, "  [prof] Total Core2: %.3f ms (len1=%zu, len2=%zu, in=%zu)\n",
                         std::chrono::duration<double, std::milli>(_p_t4 - _p_t0).count(), len1, len2, in);
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
                 fprintf(stderr, "  [prof] Core1 loop (slow): %.3f ms (%zu blocks, in=%zu)\n",
                         std::chrono::duration<double, std::milli>(_p_t4 - _p_t3).count(), blocks, in);
                 fprintf(stderr, "  [prof] Total Core2: %.3f ms (len1=%zu, len2=%zu, in=%zu)\n",
                         std::chrono::duration<double, std::milli>(_p_t4 - _p_t0).count(), len1, len2, in);
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
                    absDivNewtonCore2(dividend_span, divisor_span, quot_span);
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
            return lhs += rhs;
        }
        friend Integer operator-(Integer lhs, const Integer &rhs)
        {
            return lhs -= rhs;
        }
        friend Integer operator*(Integer lhs, const Integer &rhs)
        {
            return lhs *= rhs;
        }
        friend Integer operator/(Integer lhs, const Integer &rhs)
        {
            return lhs /= rhs;
        }
        friend Integer operator%(Integer lhs, const Integer &rhs)
        {
            return lhs %= rhs;
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
namespace {
    static char oBuffer[32 << 20], *oCursor = oBuffer;

    void writeHint(const hint::Integer& val) {
        oCursor += val.writeTo(oCursor);
    }

    void flushOutput() {
        std::fwrite(oBuffer, 1, oCursor - oBuffer, stdout);
    }
}

#if defined(HINT_OP_ADD)
int main() {
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);
	int t = 1;
	std::cin >> t;
	hint::Integer a, b;
	while (t--) {
		std::cin >> a >> b;
		writeHint(a + b);
		*oCursor++ = '\n';
	}
	flushOutput();
	return 0;
}
#elif defined(HINT_OP_MUL)
int main() {
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);
	int t = 1;
	std::cin >> t;
	hint::Integer a, b;
	while (t--) {
		std::cin >> a >> b;
		writeHint(a * b);
		*oCursor++ = '\n';
	}
	flushOutput();
	return 0;
}
#elif defined(HINT_OP_DIV)
int main() {
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);
	int t = 1;
	std::cin >> t;
	hint::Integer a, b, q, r;
	while (t--) {
		std::cin >> a >> b;
		a.absDivRem(b, q, r);
		writeHint(q);
		*oCursor++ = ' ';
		writeHint(r);
		*oCursor++ = '\n';
	}
	flushOutput();
	return 0;
}
#else
#error "Must define HINT_OP_ADD, HINT_OP_MUL, or HINT_OP_DIV"
#endif
