// fftref.cpp - FFTW 标尺 vs 我方 difRec
// 手写 fftw3 最小声明 (无 dev 头文件), 链接 libfftw3.so.3
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include <immintrin.h>

extern "C" {
typedef double fftw_complex[2];
typedef struct fftw_plan_s* fftw_plan;
fftw_plan fftw_plan_dft_1d(int n, fftw_complex* in, fftw_complex* out, int sign, unsigned flags);
void fftw_execute(const fftw_plan p);
void fftw_destroy_plan(fftw_plan p);
void* fftw_malloc(size_t n);
void fftw_free(void* p);
}
#define FFTW_FORWARD (-1)
#define FFTW_MEASURE (0u)
#define FFTW_PATIENT (1u << 5)

// ---------------- our radix-4 self-sorting DIF (copied from v9) ----------------
namespace fft {
using u32 = unsigned;
using cpx = __m128d;
static cpx* tw = nullptr;
static u32 twn = 0;
#ifndef FFT_LEAF_LOG
#define FFT_LEAF_LOG 11
#endif

static inline cpx cmul(cpx a, cpx b) {
    cpx br = _mm_unpacklo_pd(b, b), bi = _mm_unpackhi_pd(b, b);
    return _mm_fmaddsub_pd(a, br, _mm_mul_pd(_mm_shuffle_pd(a, a, 1), bi));
}
static inline __m256d BC4(cpx w) { return _mm256_set_m128d(w, w); }
static inline __m256d cmul4(__m256d a, __m256d W, __m256d Ws) {
    __m256d ar = _mm256_movedup_pd(a);
    __m256d ai = _mm256_permute_pd(a, 0xF);
    return _mm256_fmaddsub_pd(ar, W, _mm256_mul_pd(ai, Ws));
}

static void resize(u32 n) {
    if (twn >= n) return;
    if (tw) free(tw);
    tw = (cpx*)aligned_alloc(64, (size_t)n * sizeof(cpx));
    twn = n;
    tw[0] = _mm_set_pd(0.0, 1.0);
    for (u32 m = 1; m < n; m <<= 1) {
        for (u32 i = 0; i < m; ++i) {
            double ang = M_PI * (double)i / (double)(2 * m);
            tw[m + i] = _mm_set_pd(sin(ang), cos(ang));
        }
    }
}

static void difFlat(cpx* d, u32 n, u32 bb) {
    for (u32 blk = n >> 1; blk; blk >>= 1, bb <<= 1) {
        for (u32 s = 0, b = bb; s < n; s += blk << 1, ++b) {
            cpx w = tw[b];
            __m256d W = BC4(w), Ws = _mm256_permute_pd(W, 0x5);
            cpx *p = d + s, *e = p + blk;
            if (blk >= 2) {
                for (; p + 2 <= e; p += 2) {
                    __m256d x = _mm256_loadu_pd((const double*)p);
                    __m256d y = _mm256_loadu_pd((const double*)(p + blk));
                    _mm256_storeu_pd((double*)p, _mm256_add_pd(x, y));
                    _mm256_storeu_pd((double*)(p + blk), cmul4(_mm256_sub_pd(x, y), W, Ws));
                }
            }
            for (; p < e; ++p) {
                cpx x = p[0], y = p[blk];
                p[0] = _mm_add_pd(x, y);
                p[blk] = cmul(_mm_sub_pd(x, y), w);
            }
        }
    }
}
static void difRec(cpx* d, u32 n, u32 bb) {
    if (n <= (1u << FFT_LEAF_LOG)) { difFlat(d, n, bb); return; }
    const u32 q = n >> 2, bs = q << 1;
    const u32 b4 = bb << 2;
    cpx w1 = tw[bb], w2 = tw[bb << 1], w3 = tw[(bb << 1) | 1];
    __m256d W1 = BC4(w1), W1s = _mm256_permute_pd(W1, 0x5);
    __m256d W2 = BC4(w2), W2s = _mm256_permute_pd(W2, 0x5);
    __m256d W3 = BC4(w3), W3s = _mm256_permute_pd(W3, 0x5);
    for (u32 i = 0; i < q; i += 2) {
        __m256d x0 = _mm256_loadu_pd((const double*)(d + i));
        __m256d x1 = _mm256_loadu_pd((const double*)(d + i + q));
        __m256d x2 = _mm256_loadu_pd((const double*)(d + i + bs));
        __m256d x3 = _mm256_loadu_pd((const double*)(d + i + bs + q));
        __m256d s0 = _mm256_add_pd(x0, x2), d0 = _mm256_sub_pd(x0, x2);
        __m256d s1 = _mm256_add_pd(x1, x3), d1 = _mm256_sub_pd(x1, x3);
        __m256d u1 = cmul4(d1, W2, W2s);
        _mm256_storeu_pd((double*)(d + i), _mm256_add_pd(s0, s1));
        _mm256_storeu_pd((double*)(d + i + q), cmul4(_mm256_sub_pd(s0, s1), W1, W1s));
        _mm256_storeu_pd((double*)(d + i + bs), cmul4(_mm256_add_pd(d0, u1), W2, W2s));
        _mm256_storeu_pd((double*)(d + i + bs + q), cmul4(_mm256_sub_pd(d0, u1), W3, W3s));
    }
    difRec(d, q, b4);
    difRec(d + q, q, b4 | 1);
    difRec(d + 2 * q, q, b4 | 2);
    difRec(d + 3 * q, q, b4 | 3);
}
}  // namespace fft

static double now_ms() {
    using namespace std::chrono;
    return duration<double, std::milli>(high_resolution_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    int LOGN = (argc > 1) ? atoi(argv[1]) : 19;
    int REP = (argc > 2) ? atoi(argv[2]) : 15;
    const int N = 1 << LOGN;
    printf("N=2^%d=%d  data=%.1f MiB  rep=%d\n", LOGN, N, N * 16.0 / 1048576.0, REP);

    // ---- our ----
    fft::resize(N);
    fft::cpx* ours = (fft::cpx*)aligned_alloc(64, (size_t)N * 16);
    fftw_complex* fin = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
    for (int i = 0; i < N; ++i) {
        double re = (double)((i * 2654435761u) & 0xFFFF);
        double im = (double)((i * 40503u) & 0xFFFF);
        ours[i] = _mm_set_pd(im, re);
        fin[i][0] = re; fin[i][1] = im;
    }
    fftw_plan pl = fftw_plan_dft_1d(N, fin, fin, FFTW_FORWARD, FFTW_MEASURE);

    std::vector<double> to, tf;
    for (int r = 0; r < REP; ++r) {
        // ours
        double t0 = now_ms();
        fft::difRec(ours, N, 0);
        double t1 = now_ms();
        to.push_back(t1 - t0);
        // fftw
        double t2 = now_ms();
        fftw_execute(pl);
        double t3 = now_ms();
        tf.push_back(t3 - t2);
    }
    std::sort(to.begin(), to.end());
    std::sort(tf.begin(), tf.end());
    double mo = to[REP / 2], mf = tf[REP / 2];
    printf("  ours(difRec,no-bitrev) med=%.3f ms  min=%.3f\n", mo, to[0]);
    printf("  FFTW3   (full,ordered) med=%.3f ms  min=%.3f\n", mf, tf[0]);
    printf("  ratio ours/FFTW = %.3f  (<1 = we are faster)\n", mo / mf);
    // flop model: 5 N log2 N  (radix-2 equivalent upper bound)
    double flop = 5.0 * N * LOGN;
    printf("  model flop=%.1f M | ours %.2f Gflop/s | fftw %.2f Gflop/s\n",
           flop / 1e6, flop / (mo * 1e6), flop / (mf * 1e6));
    // memory traffic model: passes * 2 * N*16 bytes
    double passes = (double)LOGN / 2.0;
    double bytes = passes * 2.0 * N * 16.0;
    printf("  model L2<->L3 traffic=%.1f MiB | ours %.1f GB/s\n", bytes / 1048576.0, bytes / (mo * 1e6));
    fftw_destroy_plan(pl);
    return 0;
}
