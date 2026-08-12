#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t; using u128 = unsigned __int128;
static const u64 P2 = 167772161ULL, G = 3ULL;
static inline u64 mulmod(u64 a, u64 b, u64 p){ return (u64)((u128)a * b % p); }
static inline u64 addmod(u64 a, u64 b, u64 p){ u64 r = a + b; return r >= p ? r - p : r; }
static inline u64 submod(u64 a, u64 b, u64 p){ return a >= b ? a - b : a + p - b; }
static u64 powmod(u64 a, u64 e, u64 p){ u64 r = 1; a %= p; while (e){ if (e & 1) r = mulmod(r, a, p); a = mulmod(a, a, p); e >>= 1; } return r; }
static void ntt(vector<u64>& a, u64 p, bool invert){
    int n = (int)a.size();
    for (int i = 1, j = 0; i < n; i++){ int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) swap(a[i], a[j]); }
    for (int len = 2; len <= n; len <<= 1){
        u64 wlen = powmod(G, (p - 1) / (u64)len, p);
        if (invert) wlen = powmod(wlen, p - 2, p);
        for (int i = 0; i < n; i += len){ u64 w = 1; for (int k = 0; k < len / 2; k++){
            u64 u = a[i + k], v = mulmod(a[i + k + len / 2], w, p);
            a[i + k] = addmod(u, v, p); a[i + k + len / 2] = submod(u, v, p); w = mulmod(w, wlen, p);
        } }
    }
    if (invert){ u64 invn = powmod((u64)n, p - 2, p); for (int i = 0; i < n; i++) a[i] = mulmod(a[i], invn, p); }
}
int main(int argc, char** argv){
    int N = argc > 1 ? atoi(argv[1]) : 16384;
    mt19937_64 rng(987654321);
    // round-trip
    vector<u64> a(N); for (int i=0;i<N;i++) a[i]=rng()%P2; vector<u64> b=a;
    ntt(a,P2,false); ntt(a,P2,true);
    bool rt=true; for(int i=0;i<N;i++) if(a[i]!=b[i]){rt=false;break;}
    fprintf(stderr,"N=%d roundtrip=%s\n",N,rt?"PASS":"FAIL");
    // conv vs naive mod p
    int na=200, nb=300; vector<u64> A(na),B(nb); for(int i=0;i<na;i++)A[i]=rng()%P2; for(int i=0;i<nb;i++)B[i]=rng()%P2;
    int M=1; while(M<na+nb-1)M<<=1;
    vector<u64> fa(M,0),fb(M,0); for(int i=0;i<na;i++)fa[i]=A[i]; for(int i=0;i<nb;i++)fb[i]=B[i];
    ntt(fa,P2,false); ntt(fb,P2,false);
    for(int i=0;i<M;i++) fa[i]=mulmod(fa[i],fb[i],P2);
    ntt(fa,P2,true);
    vector<u64> nv(M,0); for(int i=0;i<na;i++)for(int j=0;j<nb;j++) nv[i+j]=(nv[i+j]+(u64)((u128)A[i]*B[j]%P2))%P2;
    size_t fb_idx=M; for(size_t i=0;i<(size_t)(na+nb-1);i++) if(fa[i]!=nv[i]){fb_idx=i;break;}
    fprintf(stderr,"N=%d conv=%s",N, fb_idx==M?"PASS":"FAIL");
    if(fb_idx!=M) fprintf(stderr," first_bad=%zu ntt=%llu nv=%llu  A0*B2=%llu\n",(unsigned long long)fb_idx,(unsigned long long)fa[fb_idx],(unsigned long long)nv[fb_idx],(unsigned long long)((u128)A[0]*B[2]%P2));
    fprintf(stderr,"\n");
    return 0;
}
