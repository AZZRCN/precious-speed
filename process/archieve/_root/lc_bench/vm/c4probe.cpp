// c4probe.cpp —— 量化 FFT 蝶形的 C2(RRII) vs C4(RRRRIIII) 布局代价。
//
// 背景: div_D19 的 dif 主循环里 mov/shuf : arith = 1.5~1.7, vpermpd 每循环 10-12 条。
// 假设: C2 = {r0,r1,i0,i1} 一个 ymm 只装 2 个复数, real/imag 各占半个 ymm, 于是
//        (a) 所有 real*real 只有 128-bit 宽度 -> SIMD 利用率 50%
//        (b) difSplit 尾部的 transform2(r2,i3) / swap(i3,r3) 跨 real/imag 分区 -> vpermpd
//       C4 = real 独占一整 ymm + imag 独占一整 ymm (4 复数) 应同时消掉 (a)(b)。
//
// 本探针只比较 *同一个 split-radix radix-4 蝶形主循环* 的指令数, 不做完整 FFT。
// 两版都真实读写内存并把结果 checksum 打印, 防止被优化掉。
//
// build: g++ -O2 -std=c++23 -march=x86-64-v3 -o c4probe c4probe.cpp
// run  : valgrind --tool=callgrind --callgrind-out-file=/dev/null ./c4probe A|B

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <immintrin.h>

static constexpr size_t N = 1 << 14; // doubles per buffer
static constexpr size_t REPS = 200;

// ---------------------------------------------------------------- C2 (现状)
template <typename Float>
struct Float2
{
    Float x0, x1;
    using F2 = Float2;
    Float2() = default;
    constexpr Float2(Float a, Float b) : x0(a), x1(b) {}
    friend constexpr F2 operator+(const F2 &l, const F2 &r) { return F2(l.x0 + r.x0, l.x1 + r.x1); }
    friend constexpr F2 operator-(const F2 &l, const F2 &r) { return F2(l.x0 - r.x0, l.x1 - r.x1); }
    friend constexpr F2 operator*(const F2 &l, const F2 &r) { return F2(l.x0 * r.x0, l.x1 * r.x1); }
};

template <typename Float>
struct Complex2
{
    using F2 = Float2<Float>;
    using C2 = Complex2;
    F2 real, imag;
    Complex2() = default;
    constexpr Complex2(F2 r, F2 i) : real(r), imag(i) {}
    constexpr C2 mul(const C2 &o) const
    {
        const F2 ii = imag * o.imag;
        const F2 ri = real * o.imag;
        return C2(real * o.real - ii, imag * o.real + ri);
    }
};

template <typename T>
static inline void transform2(T &s, T &d)
{
    T a = s, b = d;
    s = a + b;
    d = a - b;
}
template <typename T>
static inline void transform2(const T a, const T b, T &s, T &d)
{
    s = a + b;
    d = a - b;
}

template <typename Float>
static inline void difSplit(Float &r0, Float &i0, Float &r1, Float &i1,
                            Float &r2, Float &i2, Float &r3, Float &i3)
{
    transform2(r0, r2);
    transform2(i0, i2);
    transform2(r1, r3);
    transform2(i1, i3);
    transform2(r2, i3);
    transform2(i2, r3, r3, i2);
    std::swap(i3, r3);
}

using C2d = Complex2<double>;

__attribute__((noinline)) static void difLoopC2(double *inout, const double *t1,
                                                const double *t3, size_t stride1)
{
    auto it = reinterpret_cast<C2d *>(__builtin_assume_aligned(inout, 32));
    auto tp1 = reinterpret_cast<const C2d *>(__builtin_assume_aligned(t1, 32));
    auto tp3 = reinterpret_cast<const C2d *>(__builtin_assume_aligned(t3, 32));
    const size_t s2 = stride1 * 2, s3 = stride1 * 3;
    for (auto end = it + stride1; it < end; it++, tp1++, tp3++)
    {
        C2d c0 = it[0], c1 = it[stride1], c2 = it[s2], c3 = it[s3];
        difSplit(c0.real, c0.imag, c1.real, c1.imag, c2.real, c2.imag, c3.real, c3.imag);
        it[0] = c0;
        it[stride1] = c1;
        it[s2] = c2.mul(tp1[0]);
        it[s3] = c3.mul(tp3[0]);
    }
}

// ---------------------------------------------------------------- C4 (提案)
// 布局: 每 4 个复数一组, 8 double = {r0,r1,r2,r3, i0,i1,i2,i3}
// real / imag 各占一个完整 ymm -> 全宽运算, 且 real<->imag 的交换只是寄存器重命名。
struct C4
{
    __m256d re, im;
};

static inline C4 c4load(const double *p)
{
    return C4{_mm256_load_pd(p), _mm256_load_pd(p + 4)};
}
static inline void c4store(double *p, const C4 &c)
{
    _mm256_store_pd(p, c.re);
    _mm256_store_pd(p + 4, c.im);
}
static inline C4 c4mul(const C4 &a, const C4 &b)
{
    // 零 shuffle: 4 条 FMA 级指令
    return C4{_mm256_fnmadd_pd(a.im, b.im, _mm256_mul_pd(a.re, b.re)),
              _mm256_fmadd_pd(a.im, b.re, _mm256_mul_pd(a.re, b.im))};
}
static inline void t2(__m256d &s, __m256d &d)
{
    __m256d a = s, b = d;
    s = _mm256_add_pd(a, b);
    d = _mm256_sub_pd(a, b);
}

// difSplit 的 C4 版: 末尾的 std::swap 变成纯变量重命名(零指令)。
static inline void difSplitC4(C4 &c0, C4 &c1, C4 &c2, C4 &c3)
{
    t2(c0.re, c2.re);
    t2(c0.im, c2.im);
    t2(c1.re, c3.re);
    t2(c1.im, c3.im);
    // c2' = A - i*B, c3' = A + i*B  (A=c2, B=c3)  —— 见 difSplit 展开
    __m256d r2 = c2.re, i2 = c2.im, r3 = c3.re, i3 = c3.im;
    c2.re = _mm256_add_pd(r2, i3);
    c2.im = _mm256_sub_pd(i2, r3);
    c3.re = _mm256_sub_pd(r2, i3); // 原 swap(i3,r3) 在此被吸收
    c3.im = _mm256_add_pd(i2, r3);
}

__attribute__((noinline)) static void difLoopC4(double *inout, const double *t1,
                                                const double *t3, size_t stride1)
{
    // stride1 以 C4 为单位; 每个 C4 = 8 double
    const size_t s2 = stride1 * 2, s3 = stride1 * 3;
    for (size_t k = 0; k < stride1; k++)
    {
        C4 c0 = c4load(inout + k * 8);
        C4 c1 = c4load(inout + (k + stride1) * 8);
        C4 c2 = c4load(inout + (k + s2) * 8);
        C4 c3 = c4load(inout + (k + s3) * 8);
        difSplitC4(c0, c1, c2, c3);
        c4store(inout + k * 8, c0);
        c4store(inout + (k + stride1) * 8, c1);
        c4store(inout + (k + s2) * 8, c4mul(c2, c4load(t1 + k * 8)));
        c4store(inout + (k + s3) * 8, c4mul(c3, c4load(t3 + k * 8)));
    }
}

// ---------------------------------------------------------------- driver
int main(int argc, char **argv)
{
    char mode = (argc > 1) ? argv[1][0] : 'A';
    double *buf, *t1, *t3;
    if (posix_memalign((void **)&buf, 32, N * sizeof(double)) ||
        posix_memalign((void **)&t1, 32, N * sizeof(double)) ||
        posix_memalign((void **)&t3, 32, N * sizeof(double)))
        return 1;
    for (size_t i = 0; i < N; i++)
    {
        buf[i] = std::sin(double(i) * 0.001) * 1000.0;
        t1[i] = std::cos(double(i) * 0.002);
        t3[i] = std::cos(double(i) * 0.003);
    }
    // C2: stride1 以 C2(4 double) 计; C4: 以 C4(8 double) 计。两者覆盖同样多的复数。
    const size_t c2_units = N / 4, c4_units = N / 8;
    for (size_t r = 0; r < REPS; r++)
    {
        if (mode == 'A')
            difLoopC2(buf, t1, t3, c2_units / 4);
        else
            difLoopC4(buf, t1, t3, c4_units / 4);
    }
    double s = 0;
    for (size_t i = 0; i < N; i++)
        s += buf[i];
    std::printf("mode=%c checksum=%.6e\n", mode, s);
    free(buf);
    free(t1);
    free(t3);
    return 0;
}
