// Standalone concept test: 6-step cache-blocking for large power-of-2 FFT on Zen3.
// Reference = independent naive O(N^2) DFT. Primitive for 6-step = forward radix-2 DIT.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

using namespace std;
static const double PI = 3.14159265358979323846;

// ---- naive O(N^2) forward DFT (independent ground truth) ----
static void dft_naive(const vector<double> &xre, const vector<double> &xim,
                      vector<double> &yre, vector<double> &yim)
{
    int N = (int)xre.size();
    for (int k = 0; k < N; ++k)
    {
        double sr = 0, si = 0;
        for (int n = 0; n < N; ++n)
        {
            double ang = -2 * PI * n * k / N;
            double wr = cos(ang), wi = sin(ang);
            sr += xre[n] * wr - xim[n] * wi;
            si += xre[n] * wi + xim[n] * wr;
        }
        yre[k] = sr; yim[k] = si;
    }
}

// ---- iterative radix-2 DIT FFT, natural in -> natural out (reference primitive) ----
static void fft_radix2(double *re, double *im, int n, bool inverse)
{
    int j = 0;
    for (int i = 1; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { swap(re[i], re[j]); swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        double ang = (inverse ? 2 : -2) * PI / len;
        double wr = cos(ang), wi = sin(ang);
        int half = len / 2;
        for (int i = 0; i < n; i += len)
        {
            double cwr = 1, cwi = 0;
            for (int k = 0; k < half; ++k)
            {
                int a = i + k, b = i + k + half;
                double tr = re[b] * cwr - im[b] * cwi;
                double ti = re[b] * cwi + im[b] * cwr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] = re[a] + tr; im[a] = im[a] + ti;
                double ncwr = cwr * wr - cwi * wi;
                double ncwi = cwr * wi + cwi * wr;
                cwr = ncwr; cwi = ncwi;
            }
        }
    }
    if (inverse)
        for (int i = 0; i < n; ++i) { re[i] /= n; im[i] /= n; }
}

// DFT of one COLUMN (length R) of an R x C row-major matrix (col index `col`).
static void col_dft(double *re, double *im, int R, int C, int col, bool inv)
{
    vector<double> tr(R), ti(R);
    for (int r = 0; r < R; ++r) { tr[r] = re[r * C + col]; ti[r] = im[r * C + col]; }
    fft_radix2(tr.data(), ti.data(), R, inv);
    for (int r = 0; r < R; ++r) { re[r * C + col] = tr[r]; im[r * C + col] = ti[r]; }
}

// ---- General parameterized 6-step. N=R*R, idx=a*R+b (a=first/slow, b=second/fast). ----
// cfg bits: 1=input twiddle W_N^{-a*b}; 2=inter-stage twiddle W_N^{-a*b};
//           4=output twiddle W_N^{-a*b}; 8=DFT order a-first (else b-first);
//           16=transpose at end.
static void fft6g(double *re, double *im, int N, double *bre, double *bim, int cfg)
{
    int k = 0; while ((1 << k) < N) ++k;
    int R = 1 << (k / 2);
    auto tw = [&](int sign)
    {
        for (int a = 0; a < R; ++a)
            for (int b = 0; b < R; ++b)
            {
                int idx = a * R + b;
                double ang = sign * 2 * PI * a * b / N;
                double wr = cos(ang), wi = sin(ang);
                double tr = re[idx] * wr - im[idx] * wi, ti = re[idx] * wi + im[idx] * wr;
                re[idx] = tr; im[idx] = ti;
            }
    };
    auto transpose_end = [&]()
    {
        for (int a = 0; a < R; ++a)
            for (int b = 0; b < R; ++b) { bre[a * R + b] = re[b * R + a]; bim[a * R + b] = im[b * R + a]; }
        memcpy(re, bre, sizeof(double) * N); memcpy(im, bim, sizeof(double) * N);
    };
    if (cfg & 1) tw(-1);
    if (cfg & 8)
    {
        for (int b = 0; b < R; ++b) col_dft(re, im, R, R, b, false);  // DFT over a (first)
        if (cfg & 2) tw(-1);
        for (int a = 0; a < R; ++a) fft_radix2(re + a * R, im + a * R, R, false); // DFT over b (second)
    }
    else
    {
        for (int a = 0; a < R; ++a) fft_radix2(re + a * R, im + a * R, R, false); // DFT over b
        if (cfg & 2) tw(-1);
        for (int b = 0; b < R; ++b) col_dft(re, im, R, R, b, false);  // DFT over a
    }
    if (cfg & 4) tw(-1);
    if (cfg & 16) transpose_end();
}

// ---- correctness: 6-step vs naive DFT ----
static double check_vs_naive(int N, unsigned seed, int cfg = 0)
{
    mt19937 g(seed);
    uniform_real_distribution<double> d(-1, 1);
    vector<double> xre(N), xim(N);
    for (int i = 0; i < N; ++i) { xre[i] = d(g); xim[i] = d(g); }
    vector<double> yre(N), yim(N), zre(N), zim(N);
    dft_naive(xre, xim, yre, yim);                       // ground truth
    vector<double> tre(N), tim(N);
    memcpy(tre.data(), xre.data(), sizeof(double)*N);
    memcpy(tim.data(), xim.data(), sizeof(double)*N);
    vector<double> bre(N), bim(N);
    fft6g(tre.data(), tim.data(), N, bre.data(), bim.data(), cfg);
    double m = 0;
    for (int i = 0; i < N; ++i)
    {
        m = max(m, fabs(yre[i] - tre[i]));
        m = max(m, fabs(yim[i] - tim[i]));
    }
    return m;
}

static int G_CFG = 0;
static void fft6g_wrap(double *a, double *b, int n, double *eb, double *ib)
{
    fft6g(a, b, n, eb, ib, G_CFG);
}
static double bench(void (*fn)(double*,double*,int,double*,double*), double *re, double *im,
                    int N, double *bre, double *bim, int reps)
{
    fn(re, im, N, bre, bim);
    auto t0 = chrono::steady_clock::now();
    for (int r = 0; r < reps; ++r) fn(re, im, N, bre, bim);
    auto t1 = chrono::steady_clock::now();
    return chrono::duration<double>(t1 - t0).count() / reps;
}

int main(int argc, char **argv)
{
    printf("=== 6-step FFT concept validation (Zen3) ===\n");
    // Validate reference fft_radix2 vs naive at SMALL N only (naive is O(N^2)).
    for (int N : {64, 256, 1024})
    {
        mt19937 g(99u); uniform_real_distribution<double> d(-1, 1);
        vector<double> xre(N), xim(N), yre(N), yim(N), tre(N), tim(N);
        for (int i = 0; i < N; ++i) { xre[i] = d(g); xim[i] = d(g); }
        dft_naive(xre, xim, yre, yim);
        memcpy(tre.data(), xre.data(), sizeof(double)*N);
        memcpy(tim.data(), xim.data(), sizeof(double)*N);
        fft_radix2(tre.data(), tim.data(), N, false);
        double m = 0; for (int i = 0; i < N; ++i) { m = max(m, fabs(yre[i]-tre[i])); m = max(m, fabs(yim[i]-tim[i])); }
        printf("ref radix2 vs naive N=%-5d max|diff|=%.3e %s\n", N, m, m < 1e-6 ? "OK" : "FAIL");
    }
    // Validate 6-step vs naive at SMALL N.
    for (int N : {64, 256, 1024, 4096})
    {
        double md = check_vs_naive(N, 12345u);
        printf("6-step vs naive   N=%-5d max|diff|=%.3e %s\n", N, md, md < 1e-4 ? "OK" : "FAIL");
    }
    // SEARCH for the correct 6-step config (vs naive at N=256).
    printf("--- config search (N=256 vs naive) ---\n");
    int best = -1;
    for (int cfg = 0; cfg < 32; ++cfg)
    {
        mt19937 g(12345u); uniform_real_distribution<double> d(-1, 1);
        int N = 256; vector<double> xre(N), xim(N), yre(N), yim(N), tre(N), tim(N), bre(N), bim(N);
        for (int i = 0; i < N; ++i) { xre[i]=d(g); xim[i]=d(g); tre[i]=xre[i]; tim[i]=xim[i]; }
        dft_naive(xre, xim, yre, yim);
        fft6g(tre.data(), tim.data(), N, bre.data(), bim.data(), cfg);
        double m = 0; for (int i = 0; i < N; ++i){ m=max(m,fabs(yre[i]-tre[i])); m=max(m,fabs(yim[i]-tim[i])); }
        printf("  cfg=%2d (in%d mid%d out%d order%d) max|diff|=%.3e %s\n", cfg,
               (cfg&1)?1:0,(cfg&2)?1:0,(cfg&4)?1:0,(cfg&8)?1:0, m, m<1e-6?"OK":"--");
        if (m < 1e-6) best = cfg;
    }
    if (best >= 0) printf(">>> correct cfg = %d\n", best);
    G_CFG = best;
    // Validate best cfg vs radix2 (trusted) across sizes (fast).
    if (best >= 0)
    {
        printf("--- validate cfg=%d across sizes ---\n", best);
        for (int N : {16384, 65536, 262144, 1048576})
        {
            mt19937 g(5u); uniform_real_distribution<double> d(-1, 1);
            vector<double> yre(N), yim(N), zre(N), zim(N), bre(N), bim(N);
            for (int i = 0; i < N; ++i) { yre[i] = d(g); yim[i] = d(g); zre[i] = yre[i]; zim[i] = yim[i]; }
            fft_radix2(yre.data(), yim.data(), N, false);
            fft6g(zre.data(), zim.data(), N, bre.data(), bim.data(), best);
            double m = 0; for (int i = 0; i < N; ++i) { m = max(m, fabs(yre[i]-zre[i])); m = max(m, fabs(yim[i]-zim[i])); }
            printf("  cfg=%d vs radix2 N=%-7d max|diff|=%.3e %s\n", best, N, m, m < 1e-3 ? "OK" : "FAIL");
        }
    }
    int N = (argc > 1) ? atoi(argv[1]) : 1048576;
    int reps = (argc > 2) ? atoi(argv[2]) : 50;
    mt19937 g(7u);
    uniform_real_distribution<double> d(-1, 1);
    vector<double> re(N), im(N), bre(N), bim(N);
    for (int i = 0; i < N; ++i) { re[i] = d(g); im[i] = d(g); }
    double t_flat  = bench([](double*a,double*b,int n,double*eb,double*ib){ (void)eb;(void)ib; fft_radix2(a,b,n,false); }, re.data(), im.data(), N, bre.data(), bim.data(), reps);
    for (int i = 0; i < N; ++i) { re[i] = d(g); im[i] = d(g); }
    double t_6step = bench(fft6g_wrap, re.data(), im.data(), N, bre.data(), bim.data(), reps);
    printf("timing N=%d reps=%d\n", N, reps);
    printf("  flat  (radix2): %.4f ms/FFT\n", t_flat*1000);
    printf("  6-step        : %.4f ms/FFT\n", t_6step*1000);
    printf("  ratio (flat/6step): %.3fx\n", t_flat / t_6step);
    return 0;
}
