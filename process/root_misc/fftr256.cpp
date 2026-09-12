// fftr256.cpp —— FFT 向量宽度 128位(__m128d, 2 double) vs 256位(__m256d, 4 double) 沙盘
// 目的: (a) 两版对 naive DFT 数值一致 (不破 2^53 精度); (b) callgrind Ir 是否近半.
// 仅前向 radix-2 DIT, 复数布局 [re,im] 连续; 256位版每寄存器处理 2 个复数 (4 double).
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <vector>
#include <random>

#if defined(__has_include)
#  if __has_include(<valgrind/callgrind.h>)
#    include <valgrind/callgrind.h>
#  else
#    define CALLGRIND_TOGGLE_COLLECT ((void)0)
#  endif
#else
#  define CALLGRIND_TOGGLE_COLLECT ((void)0)
#endif

static double frand() { static std::mt19937_64 g(12345); return (double)(g() & 0xFFFFFF) - 0x800000; }

// ---- naive DFT (正确性基准) ----
static void dft(const std::vector<double>& re, const std::vector<double>& im,
                std::vector<double>& ro, std::vector<double>& io, int N) {
    for (int k = 0; k < N; ++k) {
        double sR = 0, sI = 0;
        double ang = -2.0 * M_PI * k / N;
        for (int n = 0; n < N; ++n) {
            double a = ang * n;
            sR += re[n] * cos(a) - im[n] * sin(a);
            sI += re[n] * sin(a) + im[n] * cos(a);
        }
        ro[k] = sR; io[k] = sI;
    }
}

// twiddle 表: tw[j] = exp(-2πi j / N) for j in [0, N/2)
static void build_tw(int N, std::vector<double>& tw) {
    tw.resize(N);  // [wr,wi,wr,wi,...] pairs, length N/2 pairs => N doubles
    for (int j = 0; j < N / 2; ++j) {
        double a = -2.0 * M_PI * j / N;
        tw[2 * j] = cos(a); tw[2 * j + 1] = sin(a);
    }
}

// ---- 128位 版: 每 butterfly 1 个复数 (__m128d) ----
static void fft128(double* a, int N, const double* tw) {
    // 位反转置换
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { for (int c = 0; c < 2; ++c) { double t = a[2*i+c]; a[2*i+c] = a[2*j+c]; a[2*j+c] = t; } }
    }
    for (int len = 2; len <= N; len <<= 1) {
        int half = len >> 1;
        int step = N / len;
        for (int block = 0; block < N; block += len) {
            for (int j = 0; j < half; ++j) {
                const double* w = tw + 2 * (j * step);
                double wr = w[0], wi = w[1];
                int p = 2 * (block + j), q = 2 * (block + j + half);
                __m128d va = _mm_loadu_pd(a + p);
                __m128d vb = _mm_loadu_pd(a + q);
                // t = b * w (complex): (br*wr - bi*wi, br*wi + bi*wr)
                __m128d bwr = _mm_set_pd(wi, wr);     // (wr, wi) -> lane0=wr lane1=wi
                __m128d bwi = _mm_set_pd(wr, wi);     // (wi, wr)
                __m128d vb_lo = _mm_shuffle_pd(vb, vb, 0); // (br,br)
                __m128d vb_hi = _mm_shuffle_pd(vb, vb, 3); // (bi,bi)
                __m128d pr = _mm_mul_pd(vb_lo, bwr);   // (br*wr, br*wi)
                __m128d pi = _mm_mul_pd(vb_hi, bwi);   // (bi*wi, bi*wr)
                __m128d t = _mm_addsub_pd(pr, pi);     // (br*wr - bi*wi, br*wi + bi*wr)
                _mm_storeu_pd(a + p, _mm_add_pd(va, t));
                _mm_storeu_pd(a + q, _mm_sub_pd(va, t));
            }
        }
    }
}

// ---- 256位 版: 每 butterfly 2 个复数 (__m256d) ----
static void fft256(double* a, int N, const double* tw) {
    // 位反转置换 (同 128 版)
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { for (int c = 0; c < 2; ++c) { double t = a[2*i+c]; a[2*i+c] = a[2*j+c]; a[2*j+c] = t; } }
    }
    for (int len = 2; len <= N; len <<= 1) {
        int half = len >> 1;
        int step = N / len;
        for (int block = 0; block < N; block += len) {
            int j = 0;
            for (; j + 1 < half; j += 2) {
                // 两复数: idx (block+j),(block+j+1) 与 (block+j+half),(block+j+1+half)
                int p0 = 2 * (block + j),     p1 = 2 * (block + j + 1);
                int q0 = 2 * (block + j + half), q1 = 2 * (block + j + 1 + half);
                __m256d va = _mm256_loadu_pd(a + p0);        // (a0r,a0i,a1r,a1i)
                __m256d vb = _mm256_loadu_pd(a + q0);        // (b0r,bji,b1r,b1i)
                // 注意: j 与 j+1 的 twiddle 在内存中间隔 step (非连续), 必须分别取再打包
                __m128d w0 = _mm_loadu_pd(tw + 2 * (j * step));        // (wr0,wi0)
                __m128d w1 = _mm_loadu_pd(tw + 2 * ((j + 1) * step));  // (wr1,wi1)
                double wr0 = w0[0], wi0 = w0[1], wr1 = w1[0], wi1 = w1[1];
                __m256d w   = _mm256_set_pd(wi1, wr1, wi0, wr0);       // lane0=wr0,lane1=wi0,lane2=wr1,lane3=wi1
                __m256d wsw = _mm256_set_pd(wr1, wi1, wr0, wi0);       // lane0=wi0,lane1=wr0,lane2=wi1,lane3=wr1
                // 展开 r/i: 必须用 vb (b 操作数) 的实部/虚部做 b*w (蝶形 t = b*w), 不是 va!
                __m256d vr = _mm256_shuffle_pd(vb, vb, 0);    // (b0r,b0r,b1r,b1r)
                __m256d vi = _mm256_shuffle_pd(vb, vb, 0xF); // (b0i,b0i,b1i,b1i)
                __m256d pr = _mm256_mul_pd(vr, w);           // (a0r*wr0,a0r*wi0,...)
                __m256d pi = _mm256_mul_pd(vi, wsw);         // (a0i*wi0,a0i*wr0,...)
                __m256d sub = _mm256_sub_pd(pr, pi);          // re = a*wr - a*wi
                __m256d add = _mm256_add_pd(pr, pi);          // im = a*wr + a*wi
                __m256d t = _mm256_blend_pd(sub, add, 0x0A);  // (re0,im0,re1,im1)
                _mm256_storeu_pd(a + p0, _mm256_add_pd(va, t));
                _mm256_storeu_pd(a + q0, _mm256_sub_pd(va, t));
            }
            for (; j < half; ++j) {  // 奇数 half 的尾项用 128 位处理
                const double* w = tw + 2 * (j * step);
                double wr = w[0], wi = w[1];
                int p = 2 * (block + j), q = 2 * (block + j + half);
                __m128d va = _mm_loadu_pd(a + p), vb = _mm_loadu_pd(a + q);
                __m128d bwr = _mm_set_pd(wi, wr), bwi = _mm_set_pd(wr, wi);
                __m128d vl = _mm_shuffle_pd(vb, vb, 0), vh = _mm_shuffle_pd(vb, vb, 3);
                __m128d pr = _mm_mul_pd(vl, bwr), pi = _mm_mul_pd(vh, bwi);
                __m128d t = _mm_addsub_pd(pr, pi);
                _mm_storeu_pd(a + p, _mm_add_pd(va, t));
                _mm_storeu_pd(a + q, _mm_sub_pd(va, t));
            }
        }
    }
}

static void run_mode(const char* mode, int N, int K, bool bench) {
    std::vector<double> tw; build_tw(N, tw);
    std::vector<double> a(N * 2);
    // 随机输入 (用确定性种子保证两版同输入)
    for (int i = 0; i < N * 2; ++i) a[i] = frand();
    std::vector<double> a0 = a;
    if (!strcmp(mode, "cmp")) {
        // 互校: 128c 与 256 算同一变换, 应逐位一致 (捕捉 twiddle/packing bug)
        std::vector<double> a1 = a0, a2 = a0;
        fft128(a1.data(), N, tw.data());
        fft256(a2.data(), N, tw.data());
        double maxdiff = 0;
        for (int i = 0; i < N * 2; ++i) { double d = fabs(a1[i] - a2[i]); if (d > maxdiff) maxdiff = d; }
        printf("cmp N=%d 128c_vs_256 maxdiff=%.3e\n", N, maxdiff);
        return;
    }
    if (!bench) {
        // 正确性: 对 DFT
        std::vector<double> re1(N), im1(N), re_in(N), im_in(N), ref_re(N), ref_im(N);
        for (int i = 0; i < N; ++i) { re_in[i] = a0[2 * i]; im_in[i] = a0[2 * i + 1]; }
        if (!strcmp(mode, "128c")) fft128(a.data(), N, tw.data()); else fft256(a.data(), N, tw.data());
        for (int i = 0; i < N; ++i) { re1[i] = a[2 * i]; im1[i] = a[2 * i + 1]; }
        dft(re_in, im_in, ref_re, ref_im, N);
        double maxdiff = 0;
        for (int i = 0; i < N; ++i) {
            double dr = re1[i] - ref_re[i], di = im1[i] - ref_im[i];
            double d = sqrt(dr * dr + di * di); if (d > maxdiff) maxdiff = d;
        }
        printf("%s N=%d maxdiff=%.3e\n", mode, N, maxdiff);
    } else {
        // bench: 纯 K 次 FFT 循环 (供 callgrind 量 Ir, 无 DFT)
        double chk = 0;
        for (int it = 0; it < K; ++it) {
            a = a0;
            if (!strcmp(mode, "128c")) fft128(a.data(), N, tw.data()); else fft256(a.data(), N, tw.data());
        }
        for (int i = 0; i < N * 2; ++i) chk += a[i];
        printf("%s N=%d K=%d bench chk=%lld\n", mode, N, K, (long long)(chk));
    }
}

int main(int argc, char** argv) {
    int N = (argc > 1) ? atoi(argv[1]) : (1 << 17);
    int K = (argc > 2) ? atoi(argv[2]) : 5;
    const char* mode = (argc > 3) ? argv[3] : "256";
    bool bench = (argc > 4) && !strcmp(argv[4], "bench");
    if (!bench && !strcmp(mode, "both")) { run_mode("128c", N, K, false); run_mode("256", N, K, false); }
    else run_mode(mode, N, K, bench);
    return 0;
}
