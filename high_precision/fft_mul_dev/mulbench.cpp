// mulbench.cpp -- 同尺寸环形卷积 (mod x^L-1) 的 FFT vs NTT 指令数对比
// 目的: 实测 NTT 能否在 260K 量级突破 double FFT 的 "2^52 精度墙" (缩小 FFT 长度/降低 Ir)。
// 两个后端计算完全相同的环形卷积; 正确性由 brute-force 环形卷积校验。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
#include <random>
#include <complex>
#include <algorithm>

#if defined(__has_include)
#  if __has_include(<valgrind/callgrind.h>)
#    include <valgrind/callgrind.h>
#  else
#    define CALLGRIND_TOGGLE_COLLECT(...) ((void)0)
#  endif
#else
#  define CALLGRIND_TOGGLE_COLLECT(...) ((void)0)
#endif

using u64 = uint64_t;
using u128 = unsigned __int128;
typedef std::complex<double> cd;

static int L, K, B;        // L = 变换长度, K = 迭代次数, B = 1<<k (基)
static int kbits;

// ---------------- 素数 / NTT 参数 ----------------
static u64 P = 0, RmodP = 0, R2 = 0, NEG_INV_P = 0;
static std::vector<u64> fwd_w, inv_w;     // NTT 旋转因子 (mont 形式)
static std::vector<cd>  fft_fwd_w, fft_inv_w; // FFT 旋转因子
static std::vector<int> rev;              // 位反转置换

// ---- Miller-Rabin (64-bit) ----
static u64 mulmod(u64 a, u64 b, u64 m){ return (u64)((u128)a*b % m); }
static u64 powmod(u64 a, u64 e, u64 m){ u64 r=1; a%=m; while(e){ if(e&1) r=mulmod(r,a,m); a=mulmod(a,a,m); e>>=1; } return r; }
static bool is_prime(u64 n){
    if(n<2) return false;
    static const u64 sp[] = {2ULL,3ULL,5ULL,7ULL,11ULL,13ULL,17ULL,19ULL,23ULL,29ULL,31ULL,37ULL};
    for(u64 p: sp){ if(n%p==0) return n==p; }
    u64 d=n-1; int s=0; while(!(d&1)){ d>>=1; ++s; }
    for(u64 a: {2ULL,325ULL,9375ULL,28178ULL,450775ULL,9780504ULL,1795265022ULL}){
        if(a%n==0) continue;
        u64 x=powmod(a,d,n); if(x==1||x==n-1) continue;
        bool ok=false; for(int r=1;r<s;++r){ x=mulmod(x,x,n); if(x==n-1){ok=true;break;} }
        if(!ok) return false;
    }
    return true;
}
// p = c*2^21 + 1 素数, p > 2^47
static void find_prime(){
    u64 c = (1ULL<<27); // 2^48/2^21 = 2^27
    if(c&1) c++;        // c 必须奇数 (p=c*2^21+1 才奇)
    while(true){
        u64 p = c*(1ULL<<21) + 1;
        if(p > (1ULL<<61)) { fprintf(stderr,"prime too big\n"); exit(1); }
        if(is_prime(p)){ P=p; fprintf(stderr,"NTT prime p=%llu (c=%llu, ~2^%.1f)\n",(unsigned long long)p,(unsigned long long)c,log2((double)p)); return; }
        c += 2;
    }
}
// 分解 c (p-1 = c*2^21) 的素因子, 用于原根检验
static std::vector<u64> factorize(u64 x){
    std::vector<u64> f;
    for(u64 p=2;p*p<=x;++p){ if(x%p==0){ f.push_back(p); while(x%p==0) x/=p; } }
    if(x>1) f.push_back(x);
    return f;
}
static u64 prim_root(u64 p){
    u64 c = (p-1)>>21; // p-1 = c*2^21
    auto fac = factorize(c); fac.push_back(2);
    std::mt19937_64 rng(12345);
    while(true){
        u64 g = 2 + rng()%(p-3);
        bool ok=true;
        for(u64 q: fac){ if(powmod(g,(p-1)/q,p)==1){ ok=false; break; } }
        if(ok) return g;
    }
}
// ---- Montgomery ----
static u64 mont_mul(u64 a, u64 b){
    u128 t = (u128)a*b;
    u64 m = (u64)t * NEG_INV_P;
    t += (u128)m * P;
    t >>= 64;
    if(t >= P) t -= P;
    return (u64)t;
}
static inline u64 to_mont(u64 x){ return mont_mul(x, R2); }
static inline u64 from_mont(u64 x){ return mont_mul(x, 1); }

static void build_tables(){
    // 位反转
    rev.assign(L,0);
    for(int i=1,j=0;i<L;++i){ int bit=L>>1; for(;j&bit;bit>>=1) j^=bit; j^=bit; if(i<j) std::swap(rev[i],rev[j]); }
    // NTT 旋转因子
    find_prime();
    u64 g = prim_root(P);
    u64 w1 = powmod(g, (P-1)/L, P);          // 本原 L 次根
    u64 w1i = powmod(w1, P-2, P);            // 逆
    fwd_w.assign(L,0); inv_w.assign(L,0);
    u64 cur=1, curi=1;
    for(int j=0;j<L;++j){ fwd_w[j]=to_mont(cur); inv_w[j]=to_mont(curi); cur=mulmod(cur,w1,P); curi=mulmod(curi,w1i,P); }
    // FFT 旋转因子
    fft_fwd_w.assign(L,cd(0,0)); fft_inv_w.assign(L,cd(0,0));
    for(int j=0;j<L;++j){
        double ang = -2.0*M_PI*j/L; fft_fwd_w[j]=cd(std::cos(ang),std::sin(ang));
        double ang2 = 2.0*M_PI*j/L; fft_inv_w[j]=cd(std::cos(ang2),std::sin(ang2));
    }
    // Montgomery 常数
    RmodP = (u64)(((u128)1 << 64) % P);
    R2 = mulmod(RmodP, RmodP, P);
    // NEG_INV_P = -P^{-1} mod 2^64
    u64 inv=1; for(int i=0;i<63;++i){ inv = inv*(2 - P*inv); } // Newton 求逆 mod 2^64
    NEG_INV_P = (u64)(0 - inv);
}

// ---------------- NTT 环形卷积 ----------------
static void ntt_iter(u64* a, bool inv){
    for(int i=0;i<L;++i) if(i<rev[i]) std::swap(a[i],a[rev[i]]);
    const std::vector<u64>& w = inv ? inv_w : fwd_w;
    for(int len=2; len<=L; len<<=1){
        int half=len>>1;
        u64 wlen = w[(L/len)%L]; // w ^ (L/len) == 本原 len 次根
        for(int i=0;i<L;i+=len){
            u64 wj = RmodP; // mont(1)
            for(int j=0;j<half;++j){
                u64 v = mont_mul(wj, a[i+j+half]);
                u64 u = a[i+j];
                a[i+j]     = u + v - (u+v>=P?P:0);
                a[i+j+half]= u - v + (u>=v?0:P);
                wj = mont_mul(wj, wlen);
            }
        }
    }
    if(inv){
        u64 invL = powmod(L%P, P-2, P);
        u64 invLm = to_mont(invL);
        for(int i=0;i<L;++i) a[i]=mont_mul(a[i], invLm);
    }
}
static void conv_ntt(const std::vector<u64>& A, const std::vector<u64>& B, std::vector<u64>& out){
    out.assign(L,0);
    std::vector<u64> fa(L), fb(L);
    for(int i=0;i<L;++i){ fa[i]=to_mont(A[i]); fb[i]=to_mont(B[i]); }
    ntt_iter(fa.data(), false); ntt_iter(fb.data(), false);
    for(int i=0;i<L;++i) fa[i]=mont_mul(fa[i], fb[i]);
    ntt_iter(fa.data(), true);
    for(int i=0;i<L;++i) out[i]=from_mont(fa[i]);
}

// ---------------- FFT 环形卷积 ----------------
// 缩放 FFT: 正向每级蝶形乘 1/sqrt(2) 使幅度有界(≈输入量级), 点乘积精确可表于 double -> 无 2^53 墙;
// 反向为普通 FFT(不缩放, 末尾 ÷L). 验: 前=(1/√L)·DFT, 点乘=(1/L)·DFT(A)·DFT(B)=(1/L)·L·DFT(A⊛B)=DFT(A⊛B), 逆普通 ÷L 还原.
static void fft_iter(cd* a, bool inv){
    for(int i=0;i<L;++i) if(i<rev[i]) std::swap(a[i],a[rev[i]]);
    const std::vector<cd>& w = inv ? fft_inv_w : fft_fwd_w;
    const double s = inv ? 1.0 : (1.0/M_SQRT2);
    for(int len=2; len<=L; len<<=1){
        int half=len>>1;
        cd wlen = w[(L/len)%L];
        for(int i=0;i<L;i+=len){
            cd wj(1,0);
            for(int j=0;j<half;++j){
                cd v = a[i+j+half]*wj*s;
                cd u = a[i+j];
                a[i+j]=u+v; a[i+j+half]=u-v;
                wj*=wlen;
            }
        }
    }
    if(inv) for(int i=0;i<L;++i) a[i]/=(double)L;
}
static void conv_fft(const std::vector<u64>& A, const std::vector<u64>& B, std::vector<u64>& out){
    out.assign(L,0);
    std::vector<cd> fa(L), fb(L);
    for(int i=0;i<L;++i){ fa[i]=(double)A[i]; fb[i]=(double)B[i]; }
    fft_iter(fa.data(), false); fft_iter(fb.data(), false);
    for(int i=0;i<L;++i) fa[i]*=fb[i];
    fft_iter(fa.data(), true);
    for(int i=0;i<L;++i){
        double x = fa[i].real();
        long long r = (long long)(x + (x>=0?0.5:-0.5)); // 就近取整
        out[i] = (u64)(r & 0x7fffffffffffffffULL); // 非负
    }
}

// ---------------- 环形进位归一 (base B, mod x^L-1) ----------------
static void normalize(std::vector<u64>& c){
    u128 carry=0;
    for(int i=0;i<L;++i){
        u128 val = (u128)c[i] + carry;
        c[i] = (u64)(val % B);
        carry = val / B;
    }
    int guard=0;
    while(carry && guard++ < L+2){
        u128 val = (u128)c[0] + carry;
        c[0] = (u64)(val % B);
        carry = val / B;
    }
}
// brute-force 环形卷积 (校验用)
static void conv_brute(const std::vector<u64>& A, const std::vector<u64>& B, std::vector<u64>& out){
    out.assign(L,0);
    for(int i=0;i<L;++i) for(int j=0;j<L;++j) out[(i+j)%L] += A[i]*B[j];
}

int main(int argc, char** argv){
    L = (argc>1)? atoi(argv[1]) : (1<<21);
    kbits = (argc>2)? atoi(argv[2]) : 13;
    K = (argc>3)? atoi(argv[3]) : 3;
    const char* mode = (argc>4)? argv[4] : "bench";
    B = 1<<kbits;
    if(L & (L-1)) { fprintf(stderr,"L must be power of 2\n"); return 1; }

    std::mt19937_64 rng(987654321);
    std::vector<u64> A(L), Bv(L);
    u64 mask = (kbits==64)? ~0ULL : ((1ULL<<kbits)-1);
    for(int i=0;i<L;++i){ A[i]=rng()&mask; Bv[i]=rng()&mask; }

    if(!strcmp(mode,"test")){
        build_tables();
        std::vector<u64> of, on, ob;
        conv_fft(A,Bv,of); normalize(of);
        conv_ntt(A,Bv,on); normalize(on);
        conv_brute(A,Bv,ob); normalize(ob);
        bool fok=true, nok=true;
        for(int i=0;i<L;++i){ if(of[i]!=ob[i]) fok=false; if(on[i]!=ob[i]) nok=false; }
        fprintf(stderr,"test L=%d k=%d : FFT==brute=%d  NTT==brute=%d\n", L, kbits, fok, nok);
        return (fok&&nok)?0:2;
    }

    if(!strcmp(mode,"cmp")){ // 大尺寸 FFT vs NTT 互校 (无 brute)
        build_tables();
        std::vector<u64> of, on;
        conv_fft(A,Bv,of); normalize(of);
        conv_ntt(A,Bv,on); normalize(on);
        int diff=0; for(int i=0;i<L;++i) if(of[i]!=on[i]) ++diff;
        fprintf(stderr,"cmp L=%d k=%d : FFT vs NTT mismatches=%d\n", L, kbits, diff);
        return diff?2:0;
    }

    // bench: 仅测量 conv 循环 (toggle 隔离, 排除建表/素数搜索)
    std::vector<u64> out(L);
    const char* be = (argc>5)? argv[5] : "fft";
    CALLGRIND_TOGGLE_COLLECT;          // default ON -> OFF (开始排除)
    build_tables();                     // OFF: 建表/素数搜索不计入
    CALLGRIND_TOGGLE_COLLECT;          // OFF -> ON (开始计数)
    if(!strcmp(be,"fft")){
        for(int it=0; it<K; ++it) conv_fft(A,Bv,out);
    } else {
        for(int it=0; it<K; ++it) conv_ntt(A,Bv,out);
    }
    CALLGRIND_TOGGLE_COLLECT;          // ON -> OFF (停止计数)
    u64 chk=0; for(int i=0;i<L;++i) chk ^= out[i];
    normalize(out);
    printf("bench done L=%d k=%d K=%d be=%s chk=%llu\n", L, kbits, K, be, (unsigned long long)chk);
    return 0;
}
