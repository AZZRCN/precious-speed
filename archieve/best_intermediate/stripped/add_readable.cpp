







#define HINT_OP_ADD




#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")

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
#include <immintrin.h>

#include <chrono>
#include <cstdio>
#ifdef PROFILE_DIV
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

    
    
    
    
    
    
#ifndef HINT_ASSUME
#define HINT_ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while(0)
#endif
#ifndef HINT_PREFETCH
#define HINT_PREFETCH(addr, rw, locality) __builtin_prefetch((addr), (rw), (locality))
#endif
#ifndef HINT_PROB_LIKELY
#define HINT_PROB_LIKELY(x, p)   __builtin_expect_with_probability(!!(x), 1, (p))
#define HINT_PROB_UNLIKELY(x, p) __builtin_expect_with_probability(!!(x), 0, (p))
#endif
#ifndef HINT_LIKELY
#define HINT_LIKELY(x)   __builtin_expect(!!(x), 1)
#define HINT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

    
    
    
    template <typename T>
    struct AlignedAlloc32
    {
        using value_type = T;
        using pointer = T *;
        using const_pointer = const T *;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        AlignedAlloc32() = default;
        template <typename U>
        AlignedAlloc32(const AlignedAlloc32<U> &) {}
        T *allocate(size_t n)
        {
            void *p = nullptr;
            #ifdef _WIN32
                p = _aligned_malloc(n * sizeof(T), 32);
                if (!p) throw std::bad_alloc();
#else
                if (posix_memalign(&p, 32, n * sizeof(T)) != 0)
                    throw std::bad_alloc();
#endif
            return static_cast<T *>(p);
        }
        void deallocate(T *p, size_t)
        {
#ifdef _WIN32
            _aligned_free(p);
#else
            free(p);
#endif
        }
        template <typename U>
        struct rebind { using other = AlignedAlloc32<U>; };
        bool operator==(const AlignedAlloc32 &) const { return true; }
        bool operator!=(const AlignedAlloc32 &) const { return false; }
    };
    template <typename T>
    using AlignedVec32 = std::vector<T, AlignedAlloc32<T>>;


    template <typename T>
    constexpr T int_ceil2(T n)
    {
        if (n <= 1)
            return 1;
        
        if constexpr (sizeof(T) <= 4)
            return T(2) << (31 - __builtin_clz(uint32_t(n - 1)));
        else
            return T(2) << (63 - __builtin_clzll(uint64_t(n - 1)));
    }

    template <typename IntTy>
    constexpr bool is_2pow(IntTy n)
    {
        return n != 0 && (n & (n - 1)) == 0;
    }

    
    template <typename T>
    constexpr int hint_log2(T n)
    {
        if (n <= 0)
            return -1;
        
        if constexpr (sizeof(T) <= 4)
            return 31 - __builtin_clz(uint32_t(n));
        else
            return 63 - __builtin_clzll(uint64_t(n));
    }
    constexpr int hint_ctz(uint32_t x)
    {
        
        if (x == 0)
            return 32;
        return __builtin_ctz(x);
    }

    constexpr int hint_ctz(uint64_t x)
    {
        
        if (x == 0)
            return 64;
        return __builtin_ctzll(x);
    }


    constexpr int hint_clz(uint32_t x)
    {
        
        if (x == 0)
            return 32;
        return __builtin_clz(x);
    }

    constexpr int hint_clz(uint64_t x)
    {
        
        if (x == 0)
            return 64;
        return __builtin_clzll(x);
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
                AlignedVec32<Float> table;  
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
                    HINT_ASSUME(is_2pow(float_len));
                    if (float_len <= 8)
                    {
                        difSmall<RIRI_IN>(inout, float_len);
                        return;
                    }
                    expand(float_len);
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    
                    
                    
                    auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                    auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                    auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        
                        HINT_PREFETCH(tp1 + 4, 0, 1);
                        HINT_PREFETCH(tp3 + 4, 0, 1);
                        C2 c0 = it[0], c1 = it[stride1], c2 = it[stride2], c3 = it[stride3];
                        if (RIRI_IN)
                        {
                            c0.permute(), c1.permute(), c2.permute(), c3.permute();
                        }
                        difSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
                        it[0] = c0, it[stride1] = c1, it[stride2] = c2.mul(tp1[0]), it[stride3] = c3.mul(tp3[0]);
                    }
                    size_t stride = float_len / 4;
                    
                    dif<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout, 32)), stride * 2);
                    dif<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 2, 32)), stride);
                    dif<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 3, 32)), stride);
                }
                template <bool RIRI_OUT>
                void idit(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));
                    if (float_len <= 8)
                    {
                        iditSmall<RIRI_OUT>(inout, float_len);
                        return;
                    }
                    expand(float_len);
                    size_t stride = float_len / 4;
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout, 32)), stride * 2);
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 2, 32)), stride);
                    idit<false>(reinterpret_cast<Float *>(__builtin_assume_aligned(inout + stride * 3, 32)), stride);
                    const size_t fft_len = float_len / 2, c2_len = fft_len / 2;
                    const size_t stride1 = c2_len / 4, stride2 = stride1 * 2, stride3 = stride1 * 3;
                    
                    auto tp1 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table1.getBegin(fft_len), 32));
                    auto tp3 = reinterpret_cast<const C2 *>(__builtin_assume_aligned(table3.getBegin(fft_len), 32));
                    auto it = reinterpret_cast<C2 *>(__builtin_assume_aligned(inout, 32));
                    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
                    {
                        
                        HINT_PREFETCH(tp1 + 4, 0, 1);
                        HINT_PREFETCH(tp3 + 4, 0, 1);
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
                HINT_ASSUME(is_2pow(float_len));
                HINT_ASSUME(float_len >= 16);
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
    
    
    
    
    
    
    
    
    
    inline void str16to4limbs(const char *p, uint16_t *limbs4)
    {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        v = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i zero = _mm_setzero_si128();
        
        __m128i lo = _mm_unpacklo_epi8(v, zero);  
        __m128i hi = _mm_unpackhi_epi8(v, zero);  
        
        __m128i mul2 = _mm_set_epi16(1, 10, 1, 10, 1, 10, 1, 10);  
        __m128i lo_s = _mm_mullo_epi16(lo, mul2);
        __m128i hi_s = _mm_mullo_epi16(hi, mul2);
        __m128i ones = _mm_set1_epi16(1);
        __m128i lo_d = _mm_madd_epi16(lo_s, ones);  
        __m128i hi_d = _mm_madd_epi16(hi_s, ones);  
        
        __m128i packed = _mm_packs_epi32(lo_d, hi_d);
        
        __m128i mul4 = _mm_set1_epi32(0x00010064);  
        __m128i v4 = _mm_madd_epi16(packed, mul4);  
        
        v4 = _mm_shuffle_epi32(v4, _MM_SHUFFLE(0, 1, 2, 3));
        
        __m128i result = _mm_packs_epi32(v4, v4);
        _mm_storel_epi64(reinterpret_cast<__m128i *>(limbs4), result);
    }
    
    
    inline void copyU16ToF64(const uint16_t *src, double *dst, size_t n)
    {
        size_t j = 0;
#if defined(__AVX2__)
        for (; j + 16 <= n; j += 16)
        {
            
            __m256i vals = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + j));
            __m128i lo128 = _mm256_castsi256_si128(vals);      
            __m128i hi128 = _mm256_extracti128_si256(vals, 1); 
            __m256i lo32 = _mm256_cvtepu16_epi32(lo128);       
            __m256i hi32 = _mm256_cvtepu16_epi32(hi128);       
            __m256d d0 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(lo32));
            __m256d d1 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(lo32, 1));
            __m256d d2 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(hi32));
            __m256d d3 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(hi32, 1));
            _mm256_storeu_pd(dst + j, d0);
            _mm256_storeu_pd(dst + j + 4, d1);
            _mm256_storeu_pd(dst + j + 8, d2);
            _mm256_storeu_pd(dst + j + 12, d3);
        }
#endif
        for (; j < n; j++)
        {
            dst[j] = src[j];
        }
    }
    
    
    inline void copyU16ToF64AndFill(const uint16_t *src, double *dst, size_t n, size_t total)
    {
        size_t j = 0;
#if defined(__AVX2__)
        
        for (; j + 16 <= n; j += 16)
        {
            __m256i vals = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + j));
            __m128i lo128 = _mm256_castsi256_si128(vals);
            __m128i hi128 = _mm256_extracti128_si256(vals, 1);
            __m256i lo32 = _mm256_cvtepu16_epi32(lo128);
            __m256i hi32 = _mm256_cvtepu16_epi32(hi128);
            __m256d d0 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(lo32));
            __m256d d1 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(lo32, 1));
            __m256d d2 = _mm256_cvtepi32_pd(_mm256_castsi256_si128(hi32));
            __m256d d3 = _mm256_cvtepi32_pd(_mm256_extracti128_si256(hi32, 1));
            _mm256_storeu_pd(dst + j, d0);
            _mm256_storeu_pd(dst + j + 4, d1);
            _mm256_storeu_pd(dst + j + 8, d2);
            _mm256_storeu_pd(dst + j + 12, d3);
        }
#endif
        for (; j < n; j++)
        {
            dst[j] = src[j];
        }
        
        j = n;
#if defined(__AVX2__)
        __m256d zero = _mm256_setzero_pd();
        for (; j + 4 <= total; j += 4)
        {
            _mm256_storeu_pd(dst + j, zero);
        }
#endif
        for (; j < total; j++)
        {
            dst[j] = 0.0;
        }
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
        
        while (length > 0 && HINT_UNLIKELY(array[length - 1] == 0))
        {
            length--;
        }
        return length;
    }

    
    
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

        
        size_t writeTo(char* out) const
        {
            
            
            
            
            
            
            
            
            
            
            
            
            
            
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
            
            size_t i = data.size() - 1;
            
            
            
            
            
#if defined(__AVX2__)
            while (i >= 8)
            {
                
                if (i >= 16)
                {
                    HINT_PREFETCH(&outTable.t[data[i - 9]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 10]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 11]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 12]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 13]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 14]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 15]], 0, 0);
                    HINT_PREFETCH(&outTable.t[data[i - 16]], 0, 0);
                }
                uint32_t* p32 = reinterpret_cast<uint32_t*>(p);
                p32[0] = outTable.t[data[i - 1]];
                p32[1] = outTable.t[data[i - 2]];
                p32[2] = outTable.t[data[i - 3]];
                p32[3] = outTable.t[data[i - 4]];
                p32[4] = outTable.t[data[i - 5]];
                p32[5] = outTable.t[data[i - 6]];
                p32[6] = outTable.t[data[i - 7]];
                p32[7] = outTable.t[data[i - 8]];
                p += 32;
                i -= 8;
            }
#endif
            
            while (i >= 4)
            {
                uint32_t* p32 = reinterpret_cast<uint32_t*>(p);
                p32[0] = outTable.t[data[i - 1]];
                p32[1] = outTable.t[data[i - 2]];
                p32[2] = outTable.t[data[i - 3]];
                p32[3] = outTable.t[data[i - 4]];
                p += 16;
                i -= 4;
            }
            
            while (i > 0)
            {
                i--;
                std::memcpy(p, &outTable.t[data[i]], 4);
                p += 4;
            }
            return size_t(p - out);
        }
        
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
            
            
            
            while (end - p_begin >= 16)
            {
                end -= 16;
                str16to4limbs(end, data.data() + i);
                i += 4;
            }
            
            while (end - p_begin >= 8)
            {
                end -= 8;
                data[i] = str4toi(end + 4);     
                data[i + 1] = str4toi(end);     
                i += 2;
            }
            
            while (end - p_begin >= 4)
            {
                end -= 4;
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
                i++;
            }
            
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
            
            for (; i + 15 < in2.size; i += 16)
            {
                __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in1.ptr + i));
                __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in2.ptr + i));
                __m256i r = _mm256_add_epi32(a, b);
                alignas(32) uint32_t tmp[8];
                _mm256_store_si256(reinterpret_cast<__m256i *>(tmp), r);
                
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
                if (borrow == 0)
                {
                    break;
                }
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            if (borrow == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
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
                if (borrow == 0)
                {
                    break;
                }
                out[i] = sub_half<Limb>(in1[i], borrow, BASE, borrow);
            }
            if (borrow == 0 && i < in1.size && out.ptr != in1.ptr)
            {
                std::memcpy(out.ptr + i, in1.ptr + i, (in1.size - i) * sizeof(Limb));
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
                    
                    if (in1.ptr == out.ptr) return false;
                    
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
            HINT_ASSUME(is_2pow(float_len));
            HINT_ASSUME(float_len >= conv_len);
            
            thread_local AlignedVec32<double> tv1, tv2;
            if (tv1.size() < float_len)
                tv1.resize(float_len);
            if (tv2.size() < float_len)
                tv2.resize(float_len);
            double *v1 = tv1.data(), *v2 = tv2.data();
            copyU16ToF64AndFill(in1.ptr, v1, len1, float_len);
            copyU16ToF64AndFill(in2.ptr, v2, len2, float_len);
#ifdef PROFILE_MUL
            auto _t_fft0 = std::chrono::high_resolution_clock::now();
#endif
            transform::fft::real_conv(v1, v2, float_len);
#ifdef PROFILE_MUL
            auto _t_carry0 = std::chrono::high_resolution_clock::now();
#endif
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                
                
                
                HINT_PREFETCH(v1 + i + 16, 0, 0);
                HINT_PREFETCH(v1 + i + 24, 0, 0);
                
                
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
#ifdef PROFILE_MUL
            auto _t_carry1 = std::chrono::high_resolution_clock::now();
            double _dt_fft = std::chrono::duration<double, std::milli>(_t_carry0 - _t_fft0).count();
            double _dt_carry = std::chrono::duration<double, std::milli>(_t_carry1 - _t_carry0).count();
            fprintf(stderr, "  [prof] fftMul: FFT=%.3fms CARRY=%.3fms conv_len=%zu fl=%zu\n", _dt_fft, _dt_carry, conv_len, float_len);
#endif
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
            HINT_ASSUME(is_2pow(float_len));
            HINT_ASSUME(float_len >= conv_len);
            
            thread_local AlignedVec32<double> tv;
            if (tv.size() < float_len)
                tv.resize(float_len);
            double *v = tv.data();
            copyU16ToF64AndFill(in.ptr, v, len, float_len);
            transform::fft::real_conv(v, v, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                
                
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
            
            thread_local AlignedVec32<double> b_dft;
            if (b_dft.size() < float_len) b_dft.resize(float_len);
            prepareDFT(small, b_dft.data(), float_len);
            
            
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
            copyU16ToF64AndFill(in.begin(), dft_buf, in.size, float_len);
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
            HINT_ASSUME(is_2pow(float_len));
            HINT_ASSUME(float_len >= conv_len);
            thread_local AlignedVec32<double> tv;
            if (tv.size() < float_len)
                tv.resize(float_len);
            double *v = tv.data();
            copyU16ToF64AndFill(a.ptr, v, a_len, float_len);
            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(float_len);
            fft.template dif<true>(v, float_len);
            transform::fft::real_dot_binrev2(v, b_dft, float_len);
            fft.template idit<true>(v, float_len);
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < conv_len; i += 8)
            {
                
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
                
                
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

            thread_local AlignedVec32<double> tv;
            if (tv.size() < 2 * m)
                tv.resize(2 * m);
            double *va = tv.data();
            double *vb = tv.data() + m;

            copyU16ToF64AndFill(a.ptr, va, a_len, m);
            copyU16ToF64AndFill(b.ptr, vb, b_len, m);

            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(m);
            fft.template dif<true>(va, m);
            fft.template dif<true>(vb, m);
            transform::fft::real_dot_binrev2(va, vb, m);
            fft.template idit<true>(va, m);

            
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

            thread_local AlignedVec32<double> tv;
            if (tv.size() < m)
                tv.resize(m);
            double *v = tv.data();

            copyU16ToF64AndFill(a.ptr, v, a_len, m);

            auto &fft = transform::fft::getSharedFFT<double>();
            fft.expand(m);
            fft.template dif<true>(v, m);
            transform::fft::real_dot_binrev2(v, b_dft, m);
            fft.template idit<true>(v, m);

            
            uint64_t carry = 0;
            size_t i = 0;
            for (; i + 7 < m; i += 8)
            {
                
                HINT_PREFETCH(v + i + 16, 0, 0);
                HINT_PREFETCH(v + i + 24, 0, 0);
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

        
        
        
        
        static void absInvNewtonGMP(View m, Span inv,
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
            
            absInvNewtonGMP(m + s, inv);
            size_t rn = k - s;        
            size_t inv0_len = rn + 1;
            Span inv0(inv.ptr, inv0_len);

            
            
            
            
            
            
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

                
                
                
                
                
                
                
                {
                    size_t sub_pos = rn + k - mn;
                    assert(sub_pos < mn);
                    Limb borrow = 1;
                    size_t j = sub_pos;
                    size_t guard = 0; 
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
                    
                    
                    if (borrow > 0)
                    {
                        
                        
                        std::fill_n(xp_mod.ptr, mn, Limb(BASE - 1));
                        xp_mod[0] = Limb(BASE - 2);
                    }
                }

                
                
                
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
                    
                    
                    
                    is_positive = false;
                    goto gmp_newton_fallback;
                }

                
                
                if (is_positive)
                {
                    
                    
                    std::copy_backward(inv.ptr, inv.ptr + rn + 1, inv.ptr + k + 1);

                    
                    thread_local std::vector<Limb> txp_full;
                    if (txp_full.size() < 2 * k + 2) txp_full.resize(2 * k + 2);
                    std::fill_n(txp_full.data(), 2 * k + 2, Limb(0));

                    
                    std::copy(xp_mod.ptr, xp_mod.ptr + mn, txp_full.data());

                    Limb *xp = txp_full.data();
                    Limb cy = 0;  

                    
                    
                    cy = xp[k];  
                    if (cy > 0)
                    {
                        
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        cy = 2;  
                        if (!borrow)
                        {
                            
                            borrow = absSub(View(xp, k), m, Span(xp, k));
                            assert(borrow && "ASSERT_CARRY: second sub must borrow");
                            cy = 3;
                        }
                    }
                    else
                    {
                        cy = 1;  
                    }

                    
                    if (absCompare(View(xp, k), m) > 0)
                    {
                        bool borrow = absSub(View(xp, k), m, Span(xp, k));
                        assert(!borrow && "ASSERT_NOCARRY");
                        cy++;
                    }

                    
                    
                    
                    
                    assert(2 * k >= 3 * rn && "xp_high and mul_out must not overlap");
                    Limb *xp_high = xp + 2 * k - rn;
                    Limb bf = (absCompare(View(xp, s), View(m.ptr, s)) > 0) ? 1 : 0;
                    for (size_t i = 0; i < rn; i++)
                    {
                        xp_high[i] = sub_half<Limb>(m.ptr[s + i], xp[s + i] + bf, BASE, bf);
                    }
                    assert(bf == 0 && "ASSERT_NOCARRY: xp_high sub overflow");

                    
                    
                    {
                        bool bf2 = absSub1(View(inv.ptr + s, rn), cy, Span(inv.ptr + s, rn));
                        assert(!bf2 && "MPN_DECR_U underflow");
                    }

                    
                    
                    {
                        View xp_high_view(xp + 2 * k - rn, rn);
                        View inv0_low_view(inv.ptr + s, rn);
                        Span mul_out(xp, 2 * rn);
                        absMul(xp_high_view, inv0_low_view, mul_out);
                    }

                    
                    
                    size_t add_len = 2 * rn - k;
                    Limb carry = 0;
                    if (add_len > 0)
                    {
                        carry = absAdd(View(xp + rn, add_len),
                                       View(xp + 2 * k - rn, add_len),
                                       Span(xp + rn, add_len)) ? 1 : 0;
                    }

                    
                    
                    {
                        Limb *xph = xp + 2 * k - rn;
                        for (size_t i = 0; i < s; i++)
                        {
                            Limb2 sum = Limb2(xp[3 * rn - k + i]) + Limb2(xph[2 * rn - k + i]) + carry;
                            inv.ptr[i] = Limb(sum % BASE);
                            carry = Limb(sum / BASE);
                        }
                    }

                    
                    
                    if (carry > 0)
                    {
                        bool cf2 = absAdd1(View(inv.ptr + s, rn), carry, Span(inv.ptr + s, rn));
                        assert(!cf2 && "MPN_INCR_U overflow into integer bit");
                    }

                    return;
                }
                
            }

        gmp_newton_fallback:
            
            
            
            
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
            
            while (absCompare(prod_span, dividend) > 0)
            {
                absSub(prod_span, divisor, prod_span);
                absSub1(qhat_span, 1, qhat_span);
            }
            
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
             
             
             int corrections = 0;
             while (absCompare(prod_span, dividend) > 0 && corrections < 5)
             {
                 absSub(prod_span, divisor, prod_span); 
                 absSub1(qhat_span, 1, qhat_span);
                 corrections++;      
             }
             absSub(dividend, prod_span, dividend); 
             dividend.size = k;
             
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

                
                
                if (divisor_high.size >= FFT_MUL_THRESHOLD)
                {
                    thread_local AlignedVec32<double> inv_dft_buf_c1, div_dft_buf_c1;
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

            
            
            
            thread_local std::vector<Limb> t_inv;
            size_t inv_size = in + 1;
            if (t_inv.size() < inv_size) t_inv.resize(inv_size);
            Span inv_span(t_inv.data(), inv_size);

            
            
            
            thread_local AlignedVec32<double> inv_dft_buf, divisor_dft_buf;
            size_t inv_float_len = int_ceil2(2 * in + 1);
            
            
            
            size_t divisor_float_len = int_ceil2(len2 + in);
            if (inv_dft_buf.size() < inv_float_len) inv_dft_buf.resize(inv_float_len);
            if (divisor_dft_buf.size() < divisor_float_len) divisor_dft_buf.resize(divisor_float_len);
            
            
            
            bool use_cyclic = false;
#ifndef DISABLE_2NXN_CYCLIC
            
            
            
            
            thread_local AlignedVec32<double> divisor_dft_mod_buf;
            
            
            
            
            
            
            size_t cyclic_m = std::max(int_ceil2(len2 + 1), int_ceil2((len2 + in) / 2 + 1));
            
            
            
            
            size_t est_blocks = (quotient.size + in - 1) / in;
            if (est_blocks > 10) {
                cyclic_m = int_ceil2(len2 + in + 1);
            }
            
            
            use_cyclic = (cyclic_m < in + len2);
            if (use_cyclic && divisor_dft_mod_buf.size() < cyclic_m) divisor_dft_mod_buf.resize(cyclic_m);
#endif

            
            if (in == len2)
            {
                prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                absInvNewton(divisor, inv_span, divisor_dft_buf.data(), divisor_float_len);
#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic) {
                    prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
                }
#endif
            }
            else
            {
                
                
                absInvNewtonGMP(divisor + (len2 - in), inv_span);
#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic) {
                    prepareDFT(divisor, divisor_dft_mod_buf.data(), cyclic_m);
                } else
#endif
                {
                    prepareDFT(divisor, divisor_dft_buf.data(), divisor_float_len);
                }
            }
            prepareDFT(inv_span, inv_dft_buf.data(), inv_float_len);

#ifdef PROFILE_DIV
            auto _mu_t1 = std::chrono::high_resolution_clock::now();
            PROF_PRINT("  [prof] absDivMu: absInvNewton+prepareDFT: %.3f ms (in=%zu, inv_fl=%zu, div_fl=%zu)\n",
                    std::chrono::duration<double, std::milli>(_mu_t1 - _mu_t0).count(), in, inv_float_len, divisor_float_len);
            auto _mu_t1b = std::chrono::high_resolution_clock::now();
#endif

            
            
            
            thread_local std::vector<Limb> tqhat, tprod;
            
            size_t qhat_len_max = 2 * in + 2;
            size_t prod_len_max = len2 + in + 1;
#ifndef DISABLE_2NXN_CYCLIC
            
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

                
                
                Span window(dividend.ptr + (qn_remaining - this_in), len2 + this_in);
                Span quot_block(quotient.ptr + (qn_remaining - this_in), this_in);

                
                Span divid_high = window + (len2 - 1);

                
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

                
                
                Span qhat_span = qhat_full + (in + 1);

                
                
                
                
                
                
                if (qhat_span.size > 0 && qhat_span[qhat_span.size - 1] != Limb(0))
                {
                    basicMul(divid_high, inv_span, qhat_full);
                }

                
                size_t prod_len = len2 + qhat_span.size;
                Span prod_span(tprod.data(), prod_len);
#ifdef PROFILE_DIV
                auto _blk_t2 = std::chrono::high_resolution_clock::now();
#endif
#ifndef DISABLE_2NXN_CYCLIC
                
                
                
                
                if (use_cyclic)
                {
                    Span prod_mod_span(tprod.data(), cyclic_m);
                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);
                    
                    
                    if (len2 + this_in > cyclic_m)
                    {
                        size_t wn = len2 + this_in - cyclic_m;
                        
                        
                        
                        
                        Span prod_low_wn(prod_mod_span.ptr, wn);
                        View rp_high_wn(window.ptr + (len2 + this_in - wn), wn);
                        bool borrow = absSub(prod_low_wn, rp_high_wn, prod_low_wn);
                        
                        
                        
                        Span prod_rest(prod_mod_span.ptr + wn, cyclic_m - wn);
                        if (borrow) borrow = absSub1(prod_rest, 1, prod_rest);
                        
                        
                        
                        size_t cmp_len = cyclic_m - len2;
                        View rp_cmp(window.ptr + len2, cmp_len);
                        View tp_cmp(tprod.data() + len2, cmp_len);
                        bool cx = (absCompare(rp_cmp, tp_cmp) < 0);
                        
                        
                        int32_t incr_signed = int32_t(cx) - int32_t(borrow);
                        
                        
                        
                        
                        
                        
                        
                        
                    }
                    
                }
                else
#endif
                {
                    fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);
                }
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

                
                
                
                
#ifdef PROFILE_DIV
                auto _blk_t4 = std::chrono::high_resolution_clock::now();
#endif
                size_t wnd_full_len = len2 + this_in;
                size_t prod_full_len;
#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic) {
                    prod_full_len = cyclic_m;
                } else
#endif
                {
                    prod_full_len = prod_len;
                }

#ifndef DISABLE_2NXN_CYCLIC
                if (use_cyclic)
                {
                
                
                
                
                
                
                
                int32_t r = int32_t(window[len2]) - int32_t(tprod[len2]);

                
                
                
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

                
                r -= int32_t(cy);

                
                
                
                int corr_cnt = 0;
                while (r != 0 && corr_cnt < 10) {
                    if (r > 0) {
                        
                        absAdd1(qhat_span, 1, qhat_span);
                        bool b = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r -= int32_t(b);  
                    } else {
                        
                        absSub1(qhat_span, 1, qhat_span);
                        bool carry = absAdd(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                        r += int32_t(carry);  
                    }
                    corr_cnt++;
                }

                
                if (absCompare(Span(tprod.data(), len2), divisor) >= 0) {
                    absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    absAdd1(qhat_span, 1, qhat_span);
                }
                }
                else
#endif
                {
                
                int corr_down = 0;
                while (corr_down < 10) {
                    
                    bool prod_gt = false;
                    
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
                
                {
                    Span tp_extra(tprod.data() + len2, this_in);
                    View wnd_extra(window.ptr + len2, this_in);
                    bool cy_high = absSub(wnd_extra, tp_extra, tp_extra);
                    if (cy) {
                        bool cy3 = absSub1(tp_extra, 1, tp_extra);
                        cy_high = cy_high || cy3;
                    }
                    
                }

                
                
                
                int corr_up = 0;
                while (corr_up < 10) {
                    bool rp_ge_div = false;
                    for (size_t i = len2; i < len2 + this_in; i++) {
                        if (tprod[i] != 0) { rp_ge_div = true; break; }
                    }
                    if (!rp_ge_div) {
                        rp_ge_div = (absCompare(Span(tprod.data(), len2), divisor) >= 0);
                    }
                    if (!rp_ge_div) break;
                    
                    bool borrow = absSub(Span(tprod.data(), len2), divisor, Span(tprod.data(), len2));
                    size_t bi = len2;
                    while (borrow && bi < len2 + this_in) {
                        if (tprod[bi] > 0) { tprod[bi]--; borrow = false; }
                        else { tprod[bi] = Limb(BASE - 1); bi++; }
                    }
                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++;
                }
                }

                
                std::copy(tprod.data(), tprod.data() + len2, window.ptr);
                window.size = len2;
#ifdef PROFILE_DIV
                auto _blk_t5 = std::chrono::high_resolution_clock::now();
                PROF_PRINT("  [prof]   block %zu: correct+sub: %.3f ms\n",
                        (qn - qn_remaining) / in,
                        std::chrono::duration<double, std::milli>(_blk_t5 - _blk_t4).count());
#endif

                
                
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

             
             thread_local AlignedVec32<double> inv_dft_buf, divisor_dft_buf;
             size_t inv_float_len = int_ceil2(len2 * 2 + 1);
             size_t divisor_float_len = int_ceil2(len2 * 2);
             bool has_divisor_dft = false;
              
              if (blocks >= 2)  
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

            
            if (blocks >= 2)  
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
                    
                    size_t qn_mu = len1 - len2;
                    size_t mu_in;
                    if (qn_mu > len2)
                    {
                        mu_in = (qn_mu - 1) / ((qn_mu - 1) / len2 + 1) + 1;
                    }
                    else if (3 * qn_mu > len2)
                    {
                        
                        
                        
                        
                        
                        size_t in2 = (qn_mu - 1) / 2 + 1;
                        size_t in4 = (qn_mu - 1) / 4 + 1;
                        if (in2 > len2) in2 = len2;
                        if (in4 > len2) in4 = len2;
#ifdef DIV_MU_IN_QUARTER
                        mu_in = in4;  
#elif defined(DIV_MU_IN_HALF)
                        mu_in = in2;  
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
                    
                    
                    
                    
                    
                    
                    bool ab_safe = (mu_in < len2) || ((quot_span.size + mu_in - 1) / mu_in <= 10);
                    if (mu_in <= len2 && ab_safe && mu_in >= 64)
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


#include <cstdio>
#include <cstring>
#ifdef __linux__
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace {
#if defined(HINT_OP_DIV)
    static constexpr size_t OBUF_SIZE = 32 << 20;  
#else
    static constexpr size_t OBUF_SIZE = 8 << 20;   
#endif
    
    
    static char oBufferFallback[OBUF_SIZE];
    static char* oBuffer = oBufferFallback;
    static char* oCursor = oBufferFallback;

    void writeHint(const hint::Integer& val) {
        oCursor += val.writeTo(oCursor);
    }

    void flushOutput() {
        
        
#ifdef __linux__
        ssize_t total = oCursor - oBuffer;
        const char* ptr = oBuffer;
        while (total > 0) {
            ssize_t n = write(STDOUT_FILENO, ptr, total);
            if (n <= 0) break;
            ptr += n;
            total -= n;
        }
#else
        std::fwrite(oBuffer, 1, oCursor - oBuffer, stdout);
#endif
    }

    
    
    static constexpr size_t IBUF_SIZE = 8 << 20;  
    static char* iBuffer = nullptr;  
    static const char *iCursor = iBuffer, *iEnd = iBuffer;

    static void initInput() {
        
        
        
        
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#ifdef __linux__
        
        
        if (oBuffer == oBufferFallback) {
            void *m = mmap(nullptr, OBUF_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (m != MAP_FAILED) {
                madvise(m, OBUF_SIZE, MADV_HUGEPAGE);
                oBuffer = static_cast<char *>(m);
                oCursor = oBuffer;
            }
        }
        
        struct stat status;
        if (fstat(STDIN_FILENO, &status) == 0 && status.st_size > 0) {
            void *p = mmap(nullptr, status.st_size, PROT_READ, MAP_PRIVATE, STDIN_FILENO, 0);
            if (p != MAP_FAILED) {
                
                madvise(p, status.st_size, MADV_SEQUENTIAL | MADV_WILLNEED);
                madvise(p, status.st_size, MADV_HUGEPAGE);
                iCursor = reinterpret_cast<const char *>(p);
                iEnd = iCursor + status.st_size;
                return;
            }
        }
#endif
        if (iBuffer == nullptr) {
            void *m = mmap(nullptr, IBUF_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (m != MAP_FAILED) {
                madvise(m, IBUF_SIZE, MADV_HUGEPAGE);
                iBuffer = static_cast<char *>(m);
            } else {
                static char fb[IBUF_SIZE];
                iBuffer = fb;
            }
        }
        size_t n = std::fread(iBuffer, 1, IBUF_SIZE - 1, stdin);
        iBuffer[n] = '\n';  
        iCursor = iBuffer;
        iEnd = iBuffer + n;
    }

    
    
    
    
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
        
        while (p + offset < iEnd && p[offset] >= 0x21) {
            offset++;
        }
        return offset;
    }

    
    static inline void parseInteger(hint::Integer &out) {
        const char *start = iCursor;
        size_t len = swarTokenLen(iCursor);
        out.fromCharRange(start, start + len);
        iCursor += len;
        
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    }

    
    
    static inline bool tryParseI64Unchecked(const char *start, size_t len, int64_t &val) {
        if (len == 0 || len > 19) return false;
        bool neg = false;
        size_t i = 0;
        if (start[0] == '-') {
            neg = true;
            i = 1;
            if (len == 1) return false;
        }
        size_t digit_len = len - i;
        if (digit_len == 0 || digit_len > 18) return false;
        
        int64_t v = 0;
        while (len - i >= 4) {
            v = v * 10000 + hint::str4toi(start + i);
            i += 4;
        }
        while (i < len) {
            v = v * 10 + (start[i] - '0');
            i++;
        }
        val = neg ? -v : v;
        return true;
    }

    
    
    static inline uint64_t parse8SWAR_u64(uint64_t u) {
        u = (u & 0x0F0F0F0F0F0F0F0FULL) * 2561ULL;
        u = ((u >> 8) & 0x00FF00FF00FF00FFULL) * 6553601ULL;
        u = ((u >> 16) & 0x0000FFFF0000FFFFULL) * 42949672960001ULL;
        return u >> 32;
    }

    
    
    
    
    
    
    static inline int64_t parsePositiveUntilNondigit(const char *s, size_t &len) {
        if (s + 8 > iEnd) {
            int64_t v = 0;
            size_t i = 0;
            while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
                v = v * 10 + (s[i] - '0');
                i++;
            }
            len = i;
            return v;
        }

        uint64_t u;
        std::memcpy(&u, s, 8);

        constexpr uint64_t cx30 = 0x3030303030303030ULL;
        uint64_t umask = u & (u + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL;

        if (umask == cx30) {
            
            uint64_t r = parse8SWAR_u64(u);
            size_t i = 8;

            if (s + 16 > iEnd) {
                while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
                    r = r * 10 + (s[i] - '0');
                    i++;
                }
                len = i;
                return (int64_t)r;
            }

            uint64_t u2;
            std::memcpy(&u2, s + 8, 8);
            uint64_t umask2 = u2 & (u2 + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL;

            if (umask2 == cx30) {
                
                uint64_t r2 = parse8SWAR_u64(u2);
                r = r * 100000000ULL + r2;
                i = 16;
                if (s[16] >= '0' && s[16] <= '9') {
                    r = r * 10 + (s[16] - '0');
                    i = 17;
                    if (s[17] >= '0' && s[17] <= '9') {
                        r = r * 10 + (s[17] - '0');
                        i = 18;
                        while (s + i + 8 <= iEnd) {
                            uint64_t d;
                            std::memcpy(&d, s + i, 8);
                            uint64_t mask = (~d) & (d - 0x2121212121212121ULL) & 0x8080808080808080ULL;
                            if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done7; }
                            i += 8;
                        }
                        while (s[i] >= '0' && s[i] <= '9') i++;
                    }
                done7:
                    len = i;
                    return (int64_t)r;
                }
                len = i;
                return (int64_t)r;
            } else {
                
                uint64_t dl2 = __builtin_ctzll(umask2 ^ cx30) >> 3;
                if (dl2 > 0) {
                    u2 <<= 64 - (dl2 << 3);
                    uint64_t r2 = parse8SWAR_u64(u2);
                    static const uint64_t p10[] = {1, 10, 100, 1000, 10000,
                                                   100000, 1000000, 10000000};
                    r = r * p10[dl2] + r2;
                    i = 8 + dl2;
                }
                len = i;
                return (int64_t)r;
            }
        }

        
        uint64_t dl = __builtin_ctzll(umask ^ cx30) >> 3;
        if (dl == 0) { len = 0; return 0; }
        u <<= 64 - (dl << 3);
        uint64_t r = parse8SWAR_u64(u);
        len = dl;
        return (int64_t)r;
    }

    
    
    
    
    
    
    
    alignas(16) static const int8_t kShufRight[17][16] = {
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,   0},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,   0,   1},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,   0,   1,   2},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,   0,   1,   2,   3},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,   0,   1,   2,   3,   4},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,-128,   0,   1,   2,   3,   4,   5},
        {-128,-128,-128,-128,-128,-128,-128,-128,-128,   0,   1,   2,   3,   4,   5,   6},
        {-128,-128,-128,-128,-128,-128,-128,-128,   0,   1,   2,   3,   4,   5,   6,   7},
        {-128,-128,-128,-128,-128,-128,-128,   0,   1,   2,   3,   4,   5,   6,   7,   8},
        {-128,-128,-128,-128,-128,-128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9},
        {-128,-128,-128,-128,-128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10},
        {-128,-128,-128,-128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11},
        {-128,-128,-128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12},
        {-128,-128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13},
        {-128,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14},
        {   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15}
    };

    
    static inline uint32_t digitMask16(__m128i v) {
        __m128i sub = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i le9 = _mm_cmpeq_epi8(_mm_min_epu8(sub, _mm_set1_epi8(9)), sub);
        return uint32_t(_mm_movemask_epi8(le9));
    }

    
    
    static inline uint64_t simdParse16(__m128i v, uint32_t dl) {
        __m128i d = _mm_sub_epi8(v, _mm_set1_epi8('0'));
        __m128i a = _mm_shuffle_epi8(
            d, _mm_load_si128(reinterpret_cast<const __m128i *>(kShufRight[dl])));
        __m128i t1 = _mm_maddubs_epi16(a, _mm_set1_epi16(0x010A));      
        __m128i t2 = _mm_madd_epi16(t1, _mm_set1_epi32(0x00010064));    
        __m128i t3 = _mm_packus_epi32(t2, t2);
        __m128i t4 = _mm_madd_epi16(t3, _mm_set1_epi32(0x00012710));    
        uint64_t hi = uint32_t(_mm_cvtsi128_si32(t4));
        uint64_t lo = uint32_t(_mm_extract_epi32(t4, 1));
        return hi * 100000000ULL + lo;
    }

    static inline int64_t parseSIMD(const char *s, size_t &len) {
        
        if (__builtin_expect(s + 32 > iEnd, 0)) {
            return parsePositiveUntilNondigit(s, len);
        }
        __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s));
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + 16));
        uint32_t m = digitMask16(v0) | (digitMask16(v1) << 16);
        
        uint32_t dl = uint32_t(__builtin_ctzll((~uint64_t(m)) | (1ULL << 32)));
        len = dl;
        if (__builtin_expect(dl <= 16, 1)) {          
            return int64_t(simdParse16(v0, dl));
        }
        if (__builtin_expect(dl <= 18, 1)) {          
            uint32_t hd = dl - 16;                    
            uint64_t hv = uint64_t(s[0] - '0');
            if (hd == 2) hv = hv * 10 + uint64_t(s[1] - '0');
            __m128i lowv = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s + hd));
            return int64_t(hv * 10000000000000000ULL + simdParse16(lowv, 16));
        }
        
        
        
        if (dl < 32) return 0;
        size_t i = 32;
        while (s + i + 8 <= iEnd) {
            uint64_t d8;
            std::memcpy(&d8, s + i, 8);
            uint64_t mk = (~d8) & (d8 - 0x2121212121212121ULL) & 0x8080808080808080ULL;
            if (mk) { i += size_t(__builtin_ctzll(mk)) / 8; len = i; return 0; }
            i += 8;
        }
        while (s + i < iEnd && s[i] >= 0x21) i++;
        len = i;
        return 0;
    }

    
    
    static inline void writeI64(int64_t val) {
        if (val == 0) { *oCursor++ = '0'; return; }
        uint64_t uv;
        bool neg = val < 0;
        if (neg) {
            *oCursor++ = '-';
            uv = uint64_t(-(val + 1)) + 1;
        } else {
            uv = uint64_t(val);
        }
        
        uint32_t limbs[5];
        int n = 0;
        while (uv >= 10000) {
            limbs[n++] = uint32_t(uv % 10000);
            uv /= 10000;
        }
        limbs[n++] = uint32_t(uv); 
        
        uint32_t high = limbs[n - 1];
        if (high < 10) {
            *oCursor++ = char('0' + high);
        } else if (high < 100) {
            *oCursor++ = char('0' + high / 10);
            *oCursor++ = char('0' + high % 10);
        } else if (high < 1000) {
            *oCursor++ = char('0' + high / 100);
            *oCursor++ = char('0' + high / 10 % 10);
            *oCursor++ = char('0' + high % 10);
        } else {
            std::memcpy(oCursor, &hint::outTable.t[high], 4);
            oCursor += 4;
        }
        
        for (int j = n - 2; j >= 0; j--) {
            std::memcpy(oCursor, &hint::outTable.t[limbs[j]], 4);
            oCursor += 4;
        }
    }

    
    
    static inline uint32_t decDigits(uint64_t x) {
        static const uint64_t kPow10[20] = {
            1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL,
            100000ULL, 1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL,
            10000000000ULL, 100000000000ULL, 1000000000000ULL,
            10000000000000ULL, 100000000000000ULL, 1000000000000000ULL,
            10000000000000000ULL, 100000000000000000ULL,
            1000000000000000000ULL, 10000000000000000000ULL};
        uint32_t lz = 63u - uint32_t(__builtin_clzll(x));   
        uint32_t d = ((lz + 1u) * 1233u) >> 12;             
        return d + uint32_t(x >= kPow10[d]) ;               
    }

    
    
    
    
    
    static inline void writeI64NL(int64_t val) {
        uint64_t uv;
        uint64_t neg = uint64_t(val < 0);
        
        int64_t m = -int64_t(neg);
        uv = uint64_t((val ^ m) - m);

        
        
        alignas(32) char sc[64];
        uint64_t q1 = uv / 100000000ULL;          
        uint32_t r1 = uint32_t(uv - q1 * 100000000ULL);
        uint32_t q2 = uint32_t(q1 / 100000000ULL); 
        uint32_t r2 = uint32_t(q1 - uint64_t(q2) * 100000000ULL);
        uint32_t g4 = q2;
        uint32_t g3 = r2 / 10000u, g2 = r2 - g3 * 10000u;
        uint32_t g1 = r1 / 10000u, g0 = r1 - g1 * 10000u;
        std::memcpy(sc + 0,  &hint::outTable.t[g4], 4);
        std::memcpy(sc + 4,  &hint::outTable.t[g3], 4);
        std::memcpy(sc + 8,  &hint::outTable.t[g2], 4);
        std::memcpy(sc + 12, &hint::outTable.t[g1], 4);
        std::memcpy(sc + 16, &hint::outTable.t[g0], 4);
        sc[20] = '\n';

        uint32_t d = (uv == 0) ? 1u : decDigits(uv);   
        sc[19 - d] = '-';
        const char *src = sc + 20 - d - neg;
        
        std::memcpy(oCursor, src, 32);
        oCursor += d + neg + 1;
    }

    
    static inline void readToken(const char *&ptr, size_t &len) {
        ptr = iCursor;
        len = swarTokenLen(iCursor);
        iCursor += len;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;
    }
}
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
    
    auto tp0 = std::chrono::high_resolution_clock::now();
    parseInteger(a);
    parseInteger(b);
    auto tp1 = std::chrono::high_resolution_clock::now();
    double t_parse = std::chrono::duration<double, std::milli>(tp1 - tp0).count();
    
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
    auto tw0 = std::chrono::high_resolution_clock::now();
    writeHint(c);
    *oCursor++ = '\n';
    flushOutput();
    auto tw1 = std::chrono::high_resolution_clock::now();
    double t_write = std::chrono::duration<double, std::milli>(tw1 - tw0).count();
    fprintf(stderr, "PARSE: %.3f  ADD_MIN: %.3f  ADD_MED: %.3f  WRITE: %.3f\n",
            t_parse, times[0], times[N/2], t_write);
    return 0;
#endif
#ifdef PROFILE_DIV
    double t_read = 0, t_parse = 0, t_write = 0, t_nl = 0;
    auto _t0 = std::chrono::high_resolution_clock::now();
#endif
    while (t--) {
        
        
#ifdef PROFILE_DIV
        auto _p0 = std::chrono::high_resolution_clock::now();
#endif
        
        
        
        
        const char *sa = iCursor;
        size_t dla;
        size_t nega = size_t(*sa == '-');   
        int64_t va = parseSIMD(sa + nega, dla);
        va = nega ? -va : va;
        iCursor = sa + nega + dla;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  

        const char *sb = iCursor;
        size_t dlb;
        size_t negb = size_t(*sb == '-');   
        int64_t vb = parseSIMD(sb + negb, dlb);
        vb = negb ? -vb : vb;
        iCursor = sb + negb + dlb;
        if (iCursor < iEnd && *iCursor < 0x21) iCursor++;  
#ifdef PROFILE_DIV
        auto _p1 = std::chrono::high_resolution_clock::now();
        t_read += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        
        
        if (dla > 0 && dla <= 18 && dlb > 0 && dlb <= 18) {
#ifdef PROFILE_DIV
            auto _p2 = std::chrono::high_resolution_clock::now();
            t_parse += std::chrono::duration<double, std::milli>(_p2 - _p1).count();
#endif
            writeI64NL(va + vb);
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p2).count();
#endif
        } else {
            a.fromCharRange(sa, sa + nega + dla);
            b.fromCharRange(sb, sb + negb + dlb);
            a += b;
            writeHint(a);
            *oCursor++ = '\n';
#ifdef PROFILE_DIV
            _p1 = std::chrono::high_resolution_clock::now();
            t_write += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
            t_parse += std::chrono::duration<double, std::milli>(_p1 - _p0).count();
#endif
        }
#ifdef PROFILE_DIV
        auto _p4 = std::chrono::high_resolution_clock::now();
        t_nl += std::chrono::duration<double, std::milli>(_p4 - _p1).count();
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
    fprintf(_pf, "[ADD profile] read=%.3fms parse=%.3fms write=%.3fms nl=%.3fms loop=%.3fms flush=%.3fms total=%.3fms\n",
            t_read, t_parse, t_write, t_nl, t_loop, t_flush, t_loop + t_flush);
    std::fclose(_pf);
#endif
    return 0;
}
