#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <immintrin.h>
static const double M_PI_ = 3.14159265358979323846;
static void build_tw(int N, std::vector<double>& tw) {
    tw.resize(N);
    for (int j=0;j<N/2;++j){double a=-2.0*M_PI_*j/N;tw[2*j]=cos(a);tw[2*j+1]=sin(a);}
}
static void bitrev(double* a,int N){
    for(int i=1,j=0;i<N;++i){int bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){for(int c=0;c<2;++c){double t=a[2*i+c];a[2*i+c]=a[2*j+c];a[2*j+c]=t;}}}
}
static void pr(const char* tag,double* a,int N){
    printf("%-10s",tag);
    for(int i=0;i<N;++i)printf(" %7.1f%+7.1f",a[2*i],a[2*i+1]);
    printf("\n");
}
static void fft128(double* a,int N,const double* tw){
    bitrev(a,N); pr("128-bitrev",a,N);
    for(int len=2;len<=N;len<<=1){int half=len>>1,step=N/len;
        for(int block=0;block<N;block+=len)for(int j=0;j<half;++j){
            const double* w=tw+2*(j*step);double wr=w[0],wi=w[1];
            int p=2*(block+j),q=2*(block+j+half);
            __m128d va=_mm_loadu_pd(a+p),vb=_mm_loadu_pd(a+q);
            __m128d bwr=_mm_set_pd(wi,wr),bwi=_mm_set_pd(wr,wi);
            __m128d vl=_mm_shuffle_pd(vb,vb,0),vh=_mm_shuffle_pd(vb,vb,3);
            __m128d pr=_mm_mul_pd(vl,bwr),pi=_mm_mul_pd(vh,bwi);
            __m128d t=_mm_addsub_pd(pr,pi);
            _mm_storeu_pd(a+p,_mm_add_pd(va,t));_mm_storeu_pd(a+q,_mm_sub_pd(va,t));
        }
        char buf[32]; snprintf(buf,sizeof buf,"128-len%d",len); pr(buf,a,N);
    }
}
static void fft256(double* a,int N,const double* tw){
    bitrev(a,N); pr("256-bitrev",a,N);
    for(int len=2;len<=N;len<<=1){int half=len>>1,step=N/len;
        for(int block=0;block<N;block+=len){
            int j=0;
            for(;j+1<half;j+=2){
                int p0=2*(block+j),q0=2*(block+j+half);
                __m256d va=_mm256_loadu_pd(a+p0),vb=_mm256_loadu_pd(a+q0);
                __m128d w0=_mm_loadu_pd(tw+2*(j*step));
                __m128d w1=_mm_loadu_pd(tw+2*((j+1)*step));
                double wr0=w0[0],wi0=w0[1],wr1=w1[0],wi1=w1[1];
                __m256d w=_mm256_set_pd(wi1,wr1,wi0,wr0);
                __m256d wsw=_mm256_set_pd(wr1,wi1,wr0,wi0);
                __m256d vr=_mm256_shuffle_pd(vb,vb,0);
                __m256d vi=_mm256_shuffle_pd(vb,vb,0xF);
                __m256d pr=_mm256_mul_pd(vr,w);
                __m256d pi=_mm256_mul_pd(vi,wsw);
                __m256d sub=_mm256_sub_pd(pr,pi);
                __m256d add=_mm256_add_pd(pr,pi);
                __m256d t=_mm256_blend_pd(sub,add,0x0A);
                _mm256_storeu_pd(a+p0,_mm256_add_pd(va,t));
                _mm256_storeu_pd(a+q0,_mm256_sub_pd(va,t));
            }
            for(;j<half;++j){
                const double* w=tw+2*(j*step);double wr=w[0],wi=w[1];
                int p=2*(block+j),q=2*(block+j+half);
                __m128d va=_mm_loadu_pd(a+p),vb=_mm_loadu_pd(a+q);
                __m128d bwr=_mm_set_pd(wi,wr),bwi=_mm_set_pd(wr,wi);
                __m128d vl=_mm_shuffle_pd(vb,vb,0),vh=_mm_shuffle_pd(vb,vb,3);
                __m128d pr=_mm_mul_pd(vl,bwr),pi=_mm_mul_pd(vh,bwi);
                __m128d t=_mm_addsub_pd(pr,pi);
                _mm_storeu_pd(a+p,_mm_add_pd(va,t));_mm_storeu_pd(a+q,_mm_sub_pd(va,t));
            }
        }
        char buf[32]; snprintf(buf,sizeof buf,"256-len%d",len); pr(buf,a,N);
    }
}
int main(){
    int N=8;
    std::vector<double> re(N),im(N);
    std::mt19937_64 g(42);
    for(int i=0;i<N;++i){re[i]=(double)(g()&0xFFF)-(double)(g()&0x7FF);im[i]=(double)(g()&0xFFF)-(double)(g()&0x7FF);}
    std::vector<double> tw; build_tw(N,tw);
    std::vector<double> a128(N*2),a256(N*2);
    for(int i=0;i<N;++i){a128[2*i]=re[i];a128[2*i+1]=im[i];a256[2*i]=re[i];a256[2*i+1]=im[i];}
    printf("IN       ");for(int i=0;i<N;++i)printf(" %7.1f%+7.1f",re[i],im[i]);printf("\n");
    fft128(a128.data(),N,tw.data());
    fft256(a256.data(),N,tw.data());
    return 0;
}
