// Clean cache-blocking 6-step FFT test (Zen3).
// Benchmark methodology: warmup + alternation on a shared hot buffer.
// Math == validated cfg=26, cache-friendly (transpose-based column FFT).
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

static void build_tw(int size, vector<double> &tre, vector<double> &tim)
{
    tre.assign(size/2, 0); tim.assign(size/2, 0);
    for (int k = 0; k < size/2; ++k) { double ang = -2*PI*k/size; tre[k]=cos(ang); tim[k]=sin(ang); }
}
// iterative radix-2 DIT, table-driven (no trig in loop)
static void fft_it(double *re, double *im, int n, const vector<double> &twre, const vector<double> &twim)
{
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n>>1;
        for (; j&bit; bit>>=1) j ^= bit;
        j ^= bit;
        if (i<j){ swap(re[i],re[j]); swap(im[i],im[j]); }
    }
    for (int len=2; len<=n; len<<=1) {
        int half=len/2, step=n/len;
        for (int i=0;i<n;i+=len)
            for (int k=0;k<half;++k) {
                int t=k*step;
                double wr=twre[t], wi=twim[t];
                int a=i+k, b=i+k+half;
                double tr=re[b]*wr-im[b]*wi, ti=re[b]*wi+im[b]*wr;
                re[b]=re[a]-tr; im[b]=im[a]-ti;
                re[a]=re[a]+tr; im[a]=im[a]+ti;
            }
    }
}
static void transpose(double *re, double *im, int R, double *bre, double *bim)
{
    const int B=32;
    for (int aa=0;aa<R;aa+=B) for (int bb=0;bb<R;bb+=B)
        for (int a=aa;a<min(aa+B,R);++a) for (int b=bb;b<min(bb+B,R);++b) {
            bre[a*R+b]=re[b*R+a]; bim[a*R+b]=im[b*R+a];
        }
    memcpy(re,bre,sizeof(double)*R*R); memcpy(im,bim,sizeof(double)*R*R);
}
static void fft6(double *re, double *im, int N, double *bre, double *bim,
                 const vector<double> &subre, const vector<double> &subim,
                 const vector<double> &twNre, const vector<double> &twNim)
{
    int k=0; while((1<<k)<N) ++k; int R=1<<(k/2);
    transpose(re,im,R,bre,bim);
    for (int a=0;a<R;++a) fft_it(re+a*R,im+a*R,R,subre,subim);   // column DFTs
    transpose(re,im,R,bre,bim);
    for (int a=0;a<R;++a) for (int b=0;b<R;++b) {                // inter-stage twiddle W_N^{-a*b}
        int idx=a*R+b, m=a*b; double wr=twNre[m], wi=twNim[m];
        double tr=re[idx]*wr-im[idx]*wi, ti=re[idx]*wi+im[idx]*wr;
        re[idx]=tr; im[idx]=ti;
    }
    for (int a=0;a<R;++a) fft_it(re+a*R,im+a*R,R,subre,subim);   // row DFTs
    transpose(re,im,R,bre,bim);
}
static void dft_naive(const vector<double> &xre, const vector<double> &xim, vector<double> &yre, vector<double> &yim)
{
    int N=(int)xre.size();
    for (int k=0;k<N;++k){ double sr=0,si=0; for(int n=0;n<N;++n){ double ang=-2*PI*n*k/N; double wr=cos(ang),wi=sin(ang); sr+=xre[n]*wr-xim[n]*wi; si+=xre[n]*wi+xim[n]*wr; } yre[k]=sr; yim[k]=si; }
}
static double check_vs_naive(int N, unsigned seed)
{
    mt19937 g(seed); uniform_real_distribution<double> d(-1,1);
    vector<double> xre(N),xim(N),yre(N),yim(N),tre(N),tim(N),bre(N),bim(N);
    for(int i=0;i<N;++i){ xre[i]=d(g); xim[i]=d(g); tre[i]=xre[i]; tim[i]=xim[i]; }
    dft_naive(xre,xim,yre,yim);
    int k=0; while((1<<k)<N) ++k; int R=1<<(k/2);
    vector<double> subre,subim,twNre(N),twNim(N); build_tw(R,subre,subim);
    for(int m=0;m<N;++m){ double ang=-2*PI*m/N; twNre[m]=cos(ang); twNim[m]=sin(ang); }
    fft6(tre.data(),tim.data(),N,bre.data(),bim.data(),subre,subim,twNre,twNim);
    double m=0; for(int i=0;i<N;++i){ m=max(m,fabs(yre[i]-tre[i])); m=max(m,fabs(yim[i]-tim[i])); }
    return m;
}
int main(int argc, char **argv)
{
    printf("=== cache-friendly 6-step FFT (Zen3) : warmup + alternation ===\n");
    for (int N : {256, 1024, 4096}) {
        double md = check_vs_naive(N, 12345u);
        printf("6-step vs naive N=%-5d max|diff|=%.3e %s\n", N, md, md < 1e-4 ? "OK" : "FAIL");
    }
    int N = (argc > 1) ? atoi(argv[1]) : 1048576;
    int reps = (argc > 2) ? atoi(argv[2]) : 40;
    int mode = (argc > 3) ? atoi(argv[3]) : 0; // 0=alt, 1=flat only, 2=6step only
    vector<double> flatre, flatim, subre, subim, twNre, twNim;
    build_tw(N, flatre, flatim);
    int k=0; while((1<<k)<N) ++k; int R=1<<(k/2);
    build_tw(R, subre, subim);
    twNre.assign(N,0); twNim.assign(N,0);
    for (int m=0;m<N;++m){ double ang=-2*PI*m/N; twNre[m]=cos(ang); twNim[m]=sin(ang); }
    mt19937 g(7u); uniform_real_distribution<double> d(-1,1);
    vector<double> re(N), im(N), bre(N), bim(N);
    for (int i=0;i<N;++i){ re[i]=d(g); im[i]=d(g); }
    // warmup
    for (int w=0;w<3;++w){ fft_it(re.data(),im.data(),N,flatre,flatim); fft6(re.data(),im.data(),N,bre.data(),bim.data(),subre,subim,twNre,twNim); }
    if (mode==1) {
        double acc=0; for(int r=0;r<reps;++r){ auto a=chrono::steady_clock::now(); fft_it(re.data(),im.data(),N,flatre,flatim); auto b=chrono::steady_clock::now(); acc+=chrono::duration<double>(b-a).count(); }
        printf("timing N=%d reps=%d mode=flat\n",N,reps); printf("  flat  : %.4f ms/FFT\n", acc/reps*1000); return 0;
    }
    if (mode==2) {
        double acc=0; for(int r=0;r<reps;++r){ auto a=chrono::steady_clock::now(); fft6(re.data(),im.data(),N,bre.data(),bim.data(),subre,subim,twNre,twNim); auto b=chrono::steady_clock::now(); acc+=chrono::duration<double>(b-a).count(); }
        printf("timing N=%d reps=%d mode=6step\n",N,reps); printf("  6-step: %.4f ms/FFT\n", acc/reps*1000); return 0;
    }
    double af=0,a6=0; int rf=0,r6=0;
    for (int r=0;r<reps;++r) {
        if (r&1) { auto a=chrono::steady_clock::now(); fft6(re.data(),im.data(),N,bre.data(),bim.data(),subre,subim,twNre,twNim); auto b=chrono::steady_clock::now(); a6+=chrono::duration<double>(b-a).count(); ++r6; }
        else     { auto a=chrono::steady_clock::now(); fft_it(re.data(),im.data(),N,flatre,flatim); auto b=chrono::steady_clock::now(); af+=chrono::duration<double>(b-a).count(); ++rf; }
    }
    printf("timing N=%d reps=%d mode=alt(warmup=3)\n",N,reps);
    printf("  flat  : %.4f ms/FFT\n", af/rf*1000);
    printf("  6-step: %.4f ms/FFT\n", a6/r6*1000);
    printf("  ratio (flat/6step): %.3fx\n", (af/rf)/(a6/r6));
    return 0;
}
