// NTT 大数乘法原型 —— 独立于 393027_opt.cpp，仅用于 B 路径可行性实测
// 3-prime CRT NTT，d=6 bit 位数，Garner 恢复（__uint128_t），正确性对 naive 验证
// 度量：VM callgrind I refs
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>

using u64 = uint64_t;
using u32 = uint32_t;
static int g_dbg=0;
static std::vector<u64> g_dig;
static std::vector<u32> g_R3[3];  // captured per-prime conv residues when g_dbg>=2

struct Prime { u32 p, g, k; };  // p = c*2^k+1, g=primitive root
static const Prime PR[3] = {
    {998244353u, 3u, 23u},
    {1004535809u, 3u, 21u},
    {469762049u, 3u, 26u},
};

static inline u32 modmul(u32 a, u32 b, u32 p){ return (u64)a*b % p; }
static u32 modpow(u32 a, u64 e, u32 p){ u32 r=1; a%=p; while(e){ if(e&1) r=modmul(r,a,p); a=modmul(a,a,p); e>>=1; } return r; }
// extended gcd: returns x with a*x ≡ 1 (mod m), 0<x<m
static u32 invmod(u32 a, u32 m){
    u32 t=0,nt=1,r=m,nr=a;
    while(nr){ u32 q=r/nr; u32 tmp=r%nr; r=nr; nr=tmp; tmp=t-q*nt; t=nt; nt=tmp; }
    return t? t : 0;
}
static inline u32 modsub(u32 a, u32 b, u32 p){ return a>=b ? a-b : a+(p-b); }

static void ntt(std::vector<u32>& a, u32 p, u32 root, bool inv){
    int n=(int)a.size(); int lg=0; while((1<<lg)<n) lg++;
    for(int i=1,j=0;i<n;++i){ int bit=n>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit; if(i<j) std::swap(a[i],a[j]); }
    for(int len=2;len<=n;len<<=1){
        u32 wlen = modpow(root, (u64)n/len, p);   // root primitive n-th root => root^(n/len) primitive len-th root
        if(inv) wlen = modpow(wlen, p-2, p);
        for(int i=0;i<n;i+=len){
            u32 w=1;
            for(int jh=0;jh<len/2;++jh){
                u32 u=a[i+jh], v=modmul(a[i+jh+len/2], w, p);
                u32 s = u+v; if(s>=p) s-=p;                // u+v mod p
                u32 d = (u>=v) ? (u-v) : (u+p-v);          // u-v mod p
                a[i+jh]=s; a[i+jh+len/2]=d;
                w=modmul(w,wlen,p);
            }
        }
    }
    if(inv){ u32 ni=modpow(n,p-2,p); for(int i=0;i<n;++i) a[i]=modmul(a[i],ni,p); }
}
static u32 root_for(const Prime& P, int L){ return modpow(P.g, (u64)(P.p-1)/L, P.p); }

// NTT multiply of two u64-limb arrays (length n) -> product (length 2n), base 2^64 limbs
static std::vector<u64> ntt_mul(const std::vector<u64>& A, const std::vector<u64>& B){
    int n=(int)A.size();
    const int d=6; const u64 DM=(1ull<<d)-1;
    // 整个多 limb 数当作一个 base-2^d 整数整体数字化：
    // 64 不被 d 整除，绝不能按 limb 分块（会错位 d 与 64 的最小公倍数余量）。
    int nd = (64*n + d - 1)/d;
    int L=1; while(L < 2*nd) L<<=1;
    // precompute Garner inverses
    u32 v1 = invmod(PR[0].p % PR[1].p, PR[1].p);          // p0^{-1} mod p1
    u64 p01m2 = (u64)PR[0].p * PR[1].p % PR[2].p;
    u32 v3 = invmod((u32)p01m2, PR[2].p);                 // (p0*p1)^{-1} mod p2
    u64 p0 = PR[0].p, p1 = PR[1].p, p01 = (u64)p0*p1;

    std::vector<u64> out(2*n, 0);
    // residues per prime
    std::vector<u32> aa(L), bb(L);
    std::vector<u32> R[3];
    auto digitize=[&](const std::vector<u64>& X, std::vector<u32>& dst){
        std::fill(dst.begin(),dst.end(),0);
        for(int t=0;t<nd;++t){
            int limb=(d*t)/64; int sh=(d*t)%64;
            u64 v=X[limb] >> sh;
            if(sh+d>64 && limb+1<n) v |= X[limb+1] << (64-sh);
            dst[t]=(u32)(v & DM);
        }
    };
    for(int pi=0;pi<3;++pi){
        u32 p=PR[pi].p; u32 rt=root_for(PR[pi], L);
        digitize(A,aa); digitize(B,bb);
        ntt(aa,p,rt,false); ntt(bb,p,rt,false);
        for(int i=0;i<L;++i) aa[i]=modmul(aa[i],bb[i],p);
        ntt(aa,p,rt,true);
        R[pi]=aa;
        if(g_dbg>=2) g_R3[pi]=aa;
    }
    // CRT combine per digit position + carry in base 2^d
    std::vector<u64> dig(2*nd + 32, 0); // digit-acc with carries, base 2^d (headroom for carry spread)
    if(g_dbg){ printf("nd=%d L=%d\n", nd, L); }
    for(int k=0;k<2*nd;++k){
        u32 r0 = (k<L)? R[0][k] : 0;
        u32 r1 = (k<L)? R[1][k] : 0;
        u32 r2 = (k<L)? R[2][k] : 0;
        u32 y1 = modmul(modsub(r1,r0,PR[1].p), v1, PR[1].p);
        // x1 = r0 + p0*y1 ; need (x1 + p0*p1*y2) ≡ r2 (mod p2)
        u64 x1mod2 = ((u64)r0 + (u64)p0*y1) % PR[2].p;
        u32 diff = modsub(r2, (u32)x1mod2, PR[2].p);
        u32 y2 = modmul(diff, v3, PR[2].p);
        unsigned __int128 x = (unsigned __int128)r0 + (unsigned __int128)p0*y1 + (unsigned __int128)p01*y2;
        // reduce position k mod 2^d, forward overflow as carry
        u64 val = dig[k] + (u64)(x & DM);
        dig[k] = val & DM;
        u64 carry = (u64)(x >> d) + (val >> d);
        int pos = k+1;
        while(carry){ u64 v = dig[pos] + (carry & DM); dig[pos] = v & DM; carry = (carry >> d) + (v >> d); ++pos; }
        if(g_dbg>=2) printf("dig[%d]=%llu\n", k, (unsigned long long)dig[k]);
    }
    if(g_dbg>=2) g_dig = dig;
    // regroup digits (base 2^d) -> u64 limbs (base 2^64), use 128-bit window
    u64 acc=0; int abits=0; int oi=0;
    unsigned __int128 win=0; int wbits=0;
    for(int k=0;k<(int)dig.size() && oi<2*n;++k){
        win |= (unsigned __int128)dig[k] << wbits; wbits += d;
        while(wbits >= 64 && oi < 2*n){ out[oi++] = (u64)win; win >>= 64; wbits -= 64; }
    }
    if(wbits>0 && oi<2*n) out[oi++] = (u64)win;
    return out;
}

// naive schoolbook for correctness check
static std::vector<u64> naive_mul(const std::vector<u64>& A, const std::vector<u64>& B){
    int n=(int)A.size(); std::vector<u64> out(2*n,0);
    for(int i=0;i<n;++i){ unsigned __int128 c=0; for(int j=0;j<n;++j){ c += (unsigned __int128)A[i]*B[j] + out[i+j]; out[i+j]=(u64)c; c>>=64; } out[i+n]=(u64)c; }
    return out;
}

static bool eq(const std::vector<u64>& a, const std::vector<u64>& b){
    if(a.size()!=b.size()) return false;
    for(size_t i=0;i<a.size();++i) if(a[i]!=b[i]) return false;
    return true;
}

int main(int argc, char** argv){
    std::mt19937_64 rng(12345);
    int mode = argc>1 ? atoi(argv[1]) : 0;
    if(mode==0){
        int ns[]={1,2,4,8,16,64,256,1000,2000};
        for(int zi=0;zi<(int)(sizeof(ns)/sizeof(ns[0]));++zi){
            int n=ns[zi];
            std::vector<u64> A(n),B(n);
            for(int i=0;i<n;++i){ A[i]=rng(); B[i]=rng(); }
            auto R=ntt_mul(A,B); auto N=naive_mul(A,B);
            if(eq(R,N)){ printf("n=%d PASS\n", n); }
            else { int lim=(int)std::max(R.size(),N.size()); int fi=-1; for(int i=0;i<lim;++i) if(R[i]!=N[i]){fi=i;break;} printf("n=%d FAIL firstdiff limb %d (ntt=%016llx naive=%016llx)\n", n, fi, (unsigned long long)R[fi], (unsigned long long)N[fi]); }
        }
        return 0;
    }
    if(mode==10){
        // minimal convolution test: a=[1,2], b=[3,4], L=4 -> cyclic conv [3,10,8,0]
        int L=4; u32 p=998244353;
        std::vector<u32> a(L,0), b(L,0); a[0]=1;a[1]=2; b[0]=3;b[1]=4;
        u32 rt=root_for(PR[0], L);
        printf("rt=%u rt^2=%u (expect -1=%u)\n", rt, (u32)((u64)rt*rt%p), p-1);
        ntt(a,p,rt,false); ntt(b,p,rt,false);
        printf("fwd a=[%u %u %u %u] (expect A0=3 A2=-1)\n", a[0],a[1],a[2],a[3]);
        for(int i=0;i<L;++i) a[i]=modmul(a[i],b[i],p);
        printf("pointwise a=[%u %u %u %u]\n", a[0],a[1],a[2],a[3]);
        ntt(a,p,rt,true);
        printf("conv L=4: [%u %u %u %u] expect [3 10 8 0]\n", a[0],a[1],a[2],a[3]);
        printf("conv L=4: [%u %u %u %u] expect [3 10 8 0]\n", a[0],a[1],a[2],a[3]);
        return 0;
    }
    if(mode==11){
        int n=1; const int d=6; const u64 DM=(1ull<<d)-1;
        int ndig=(64+d-1)/d; int nd=n*ndig; int L=1; while(L<2*nd)L<<=1;
        u64 av=0x123456789abcdef0ULL, bv=0xfedcba9876543210ULL;
        auto fill=[&](u64 v){ std::vector<u32> x(L,0); for(int i=0;i<n;++i){ for(int t=0;t<ndig;++t){ x[i*ndig+t]=(u32)(v&DM); v>>=d; } } return x; };
        u32 rt0=root_for(PR[0],L),rt1=root_for(PR[1],L),rt2=root_for(PR[2],L);
        u32 p0=PR[0].p,p1=PR[1].p,p2=PR[2].p;
        std::vector<u32> R[3];
        for(int pi=0;pi<3;++pi){
            u32 p=PR[pi].p, rt=(pi==0?rt0:pi==1?rt1:rt2);
            auto aa=fill(av),bb=fill(bv);
            ntt(aa,p,rt,false); ntt(bb,p,rt,false);
            for(int i=0;i<L;++i) aa[i]=modmul(aa[i],bb[i],p);
            ntt(aa,p,rt,true);
            R[pi]=aa;
        }
        u32 v1=invmod(p0%p1,p1);
        u64 p01m2=(u64)p0*p1%p2; u32 v3=invmod((u32)p01m2,p2);
        u64 p0u=p0,p1u=p1,p01=(u64)p0*p1;
        // true conv digits in base 2^6
        u64 A[11],B[11]; for(int t=0;t<11;++t){A[t]=(av>>(6*t))&DM;B[t]=(bv>>(6*t))&DM;}
        for(int k=0;k<5;++k){
            u64 truth=0; for(int j=0;j<=k;++j) if(j<11&&k-j<11) truth+=A[j]*B[k-j];
            u32 r0=R[0][k],r1=R[1][k],r2=R[2][k];
            u32 y1=modmul(modsub(r1,r0,p1),v1,p1);
            u64 x1mod2=((u64)r0+(u64)p0*y1)%p2;
            u32 diff=modsub(r2,(u32)x1mod2,p2);
            u32 y2=modmul(diff,v3,p2);
            unsigned __int128 x=(unsigned __int128)r0+(unsigned __int128)p0u*y1+(unsigned __int128)p01*y2;
            printf("k=%d truth=%llu crt=%llu match=%d\n", k,(unsigned long long)truth,(unsigned long long)(u64)x, truth==(u64)x);
        }
        return 0;
    }
    if(mode==14){
        // round-trip at L=64
        int L=64;
        std::vector<u32> a(L);
        for(int i=0;i<L;++i) a[i]=(i*2654435761u)%PR[0].p;
        auto b=a;
        u32 rt=root_for(PR[0], L);
        ntt(b, PR[0].p, rt, false);
        ntt(b, PR[0].p, rt, true);
        u32 ninv = modpow(L, PR[0].p-2, PR[0].p);
        bool ok=true;
        for(int i=0;i<L;++i){ u32 exp=(u64)a[i]*L%PR[0].p; u32 got=(u64)b[i]*ninv%PR[0].p; if(got!=exp){ ok=false; if(i<6) printf("  rt fail [%d] got=%u exp=%u\n",i,got,exp);} }
        printf("NTT round-trip L=64: %s\n", ok?"PASS":"FAIL");
        return ok?0:1;
    }
    if(mode==13){
        int n=2; const int d=6; const u64 DM=(1ull<<d)-1;
        std::vector<u64> A(n),B(n);
        A[0]=0x123456789abcdef0ULL; A[1]=0x0f1e2d3c4b5a6978ULL;
        B[0]=0xfedcba9876543210ULL; B[1]=0x13579bdf2468ace0ULL;
        // correct expected 4-limb product via 128-bit schoolbook
        u64 E[4]={0,0,0,0};
        auto add128=[&](int pos, unsigned __int128 v){
            for(int p=pos; v && p<4; ++p){ unsigned __int128 s=(unsigned __int128)E[p]+v; E[p]=(u64)s; v=s>>64; }
        };
        for(int i=0;i<n;++i) for(int j=0;j<n;++j) add128(i+j, (unsigned __int128)A[i]*B[j]);
        g_dbg=2;
        auto R=ntt_mul(A,B);
        // expected base-2^6 digits from E
        std::vector<u64> edig(50,0);
        for(int l=0;l<4;++l){ unsigned __int128 v=E[l]; for(int t=0;t<11;++t) edig[l*11+t]=(u64)((v>>(6*t))&DM); }
        for(int k=0;k<44;++k) if((unsigned long long)g_dig[k]!=edig[k]) printf("MISMATCH k=%d ntt=%llu exp=%llu\n",k,(unsigned long long)g_dig[k],(unsigned long long)edig[k]);
        printf("out ntt=%016llx %016llx %016llx %016llx\n",(unsigned long long)R[0],(unsigned long long)R[1],(unsigned long long)R[2],(unsigned long long)R[3]);
        printf("out exp=%016llx %016llx %016llx %016llx\n",(unsigned long long)E[0],(unsigned long long)E[1],(unsigned long long)E[2],(unsigned long long)E[3]);
        return 0;
    }
    if(mode==12){
        g_dbg=1;
        int n=1;
        std::vector<u64> A(1,0x123456789abcdef0ULL), B(1,0xfedcba9876543210ULL);
        auto R=ntt_mul(A,B);
        printf("ntt_mul out[0]=%016llx out[1]=%016llx\n", (unsigned long long)R[0],(unsigned long long)R[1]);
        // expected base-2^6 digits of A*B
        unsigned __int128 P=(unsigned __int128)A[0]*B[0];
        printf("expect[0]=%016llx expect[1]=%016llx\n", (unsigned long long)(u64)P, (unsigned long long)(u64)(P>>64));
        return 0;
    }
    if(mode==9){
        // n=1 focused CRT test
        int n=1;
        u64 av=0x123456789abcdef0ULL, bv=0xfedcba9876543210ULL;
        std::vector<u64> A(1,av),B(1,bv);
        int d=6; int ndig=(64+d-1)/d; int nd=n*ndig; int L=1; while(L<2*nd)L<<=1;
        u32 rt0=root_for(PR[0],L), rt1=root_for(PR[1],L), rt2=root_for(PR[2],L);
        auto fill=[&](u64 v){ std::vector<u32> x(L,0); for(int i=0;i<n;++i){ for(int t=0;t<ndig;++t){ x[i*ndig+t]=(u32)(v&((1ull<<d)-1)); v>>=d; } } return x; };
        std::vector<u32> aa=fill(av), bb=fill(bv);
        printf("aa[0..10] digits: "); for(int t=0;t<ndig;++t) printf("%u ", (unsigned)((av>>(6*t))&0x3f)); printf("\n");
        std::vector<u32> af=aa, bf=bb;
        ntt(af,PR[0].p,rt0,false); ntt(bf,PR[0].p,rt0,false);
        printf("after fwd: af[0]=%u (expect sum digits=%u) af[1]=%u\n", af[0], (unsigned)((av&0x3f)+((av>>6)&0x3f)), af[1]);
        for(int i=0;i<L;++i) aa[i]=modmul(af[i],bf[i],PR[0].p);
        ntt(aa,PR[0].p,rt0,true);
        printf("c_0=%u c_1=%u c_2=%u\n", aa[0], aa[1], aa[2]);
        // c_0 from prime 0
        printf("c_0 (prime0) = %u   true a_0*b_0 = %llu\n", aa[0], (unsigned long long)((av&0x3f)*((unsigned long long)(bv&0x3f))));
        // full c_0 via CRT (just one prime needed since < 2^12)
        printf("true product low12 = %llx\n", (unsigned long long)((av*bv)&0xfff));
        return 0;
    }
    if(mode==8){
        // NTT round-trip self-test
        int L=8;
        std::vector<u32> a(L);
        for(int i=0;i<L;++i) a[i]=(i*2654435761u)%PR[0].p;
        auto b=a;
        u32 rt=root_for(PR[0], L);
        ntt(b, PR[0].p, rt, false);
        ntt(b, PR[0].p, rt, true);
        u32 ninv = modpow(L, PR[0].p-2, PR[0].p);
        bool ok=true;
        for(int i=0;i<L;++i){ u32 exp=a[i]; u32 got=b[i]; if(got!=exp){ ok=false; printf("  rt fail [%d] got=%u exp=%u\n",i,got,exp);} }
        printf("NTT round-trip: %s\n", ok?"PASS":"FAIL");
        return ok?0:1;
    }
    if(mode==7){
        // tiny debug
        int n=4;
        std::vector<u64> A(n),B(n);
        for(int i=0;i<n;++i){ A[i]=rng(); B[i]=rng(); }
        auto R=ntt_mul(A,B); auto N=naive_mul(A,B);
        printf("n=4 ntt size=%d naive size=%d eq=%d\n", (int)R.size(),(int)N.size(), eq(R,N));
        for(int i=0;i<(int)R.size();++i) printf("  [%d] ntt=%016llx naive=%016llx %s\n", i,(unsigned long long)R[i],(unsigned long long)N[i], R[i]==N[i]?"":"<--");
        return 0;
    }
    if(mode==15){
        // 正确 digit 级对照：真值 base-2^6 展开 vs g_dig
        int n=2; const int d=6; const u64 DM=(1ull<<d)-1;
        std::vector<u64> A(n),B(n);
        A[0]=0x123456789abcdef0ULL; A[1]=0x0f1e2d3c4b5a6978ULL;
        B[0]=0xfedcba9876543210ULL; B[1]=0x13579bdf2468ace0ULL;
        // 真值 schoolbook -> T[2n] base 2^64
        std::vector<u64> T(2*n,0);
        for(int i=0;i<n;++i){ unsigned __int128 c=0; for(int j=0;j<n;++j){ c+=(unsigned __int128)A[i]*B[j]+T[i+j]; T[i+j]=(u64)c; c>>=64; } T[i+n]=(u64)c; }
        auto dig_at=[&](int k)->u64{ // 真值 base-2^6 digit k
            int bit0=6*k; int lo=bit0/64; int sh=bit0%64;
            unsigned __int128 w = T[lo]; if(sh>0 && lo+1<(int)T.size()) w |= (unsigned __int128)T[lo+1]<<64;
            w >>= sh; return (u64)(w & DM);
        };
        g_dbg=2;
        auto R=ntt_mul(A,B);
        // naive digit-polynomial conv for cross-check
        int ndig=(64+d-1)/d;
        auto digits=[&](const std::vector<u64>&X, std::vector<u32>&out){
            out.assign(n*ndig,0);
            for(int i=0;i<n;++i){ u64 v=X[i]; for(int t=0;t<ndig;++t){ out[i*ndig+t]=(u32)(v&DM); v>>=d; } }
        };
        std::vector<u32> aa,bb; digits(A,aa); digits(B,bb);
        auto cnaive=[&](int k)->u32{ u32 s=0; for(int j=0;j<n*ndig;++j){ int o=k-j; if(o>=0&&o<n*ndig) s+=(u32)aa[j]*bb[o]; } return s; };
        for(int k=8;k<=14;++k){
            u32 r0=g_R3[0][k],r1=g_R3[1][k],r2=g_R3[2][k];
            printf("k=%d  R0=%u R1=%u R2=%u  naive_c=%u  dig_ntt=%llu dig_tru=%llu\n", k, r0,r1,r2, cnaive(k),
                   (unsigned long long)(k<(int)g_dig.size()?g_dig[k]:0), (unsigned long long)dig_at(k));
        }
        int maxk=2*n*((64+d-1)/d);
        int bad=0;
        for(int k=0;k<maxk;++k){ u64 t=dig_at(k); u64 g=(k<(int)g_dig.size())?g_dig[k]:0; if(t!=g){ if(bad<12) printf("  dig[%d] ntt=%llu truth=%llu\n",k,(unsigned long long)g,(unsigned long long)t); ++bad; } }
        printf("digit mismatch count=%d (of %d)\n", bad, maxk);
        printf("out ntt =%016llx %016llx %016llx %016llx\n",(unsigned long long)R[0],(unsigned long long)R[1],(unsigned long long)R[2],(unsigned long long)R[3]);
        printf("out tru =%016llx %016llx %016llx %016llx\n",(unsigned long long)T[0],(unsigned long long)T[1],(unsigned long long)T[2],(unsigned long long)T[3]);
        return 0;
    }
    if(mode==20){
        // callgrind-friendly loop: argv[2]=n (limbs), argv[3]=iters
        int n = argc>2? atoi(argv[2]) : 1024;
        int iters = argc>3? atoi(argv[3]) : 50;
        std::vector<u64> A(n),B(n);
        for(int i=0;i<n;++i){ A[i]=rng(); B[i]=rng(); }
        std::vector<u64> R; u64 sink=0;
        for(int t=0;t<iters;++t){ R=ntt_mul(A,B); sink ^= R.back(); if(t&1){ for(int i=0;i<n;++i){ A[i]=rng(); B[i]=rng(); } } }
        printf("ntt_mul n=%d iters=%d sink=%016llx\n", n, iters, (unsigned long long)sink);
        return 0;
    }
    // benchmark: n = 2^mode (mode 12..16)
    int n = 1<<mode;
    std::vector<u64> A(n),B(n);
    for(int i=0;i<n;++i){ A[i]=rng(); B[i]=rng(); }
    auto R=ntt_mul(A,B);
    // touch result to avoid DCE
    printf("bench n=%d done, top limb=%016llx\n", n, (unsigned long long)R.back());
    return 0;
}
