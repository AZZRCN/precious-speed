// Debug harness: run exact ntt_mul transform on a real input, compare
// per-prime NTT convolution vs per-prime naive (mod p) to isolate the bug.
#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t;
using u128 = unsigned __int128;

static const u64 P0 = 998244353ULL, P1 = 1004535809ULL, P2 = 167772161ULL;
static const u64 G  = 3ULL;
static const int L  = 27;
static const u64 MASKL = (1ULL << L) - 1;
static const u128 ONE64 = (u128)1 << 64;
static const u64 MASK64 = 0xFFFFFFFFFFFFFFFFULL;

static inline u64 mulmod(u64 a, u64 b, u64 p){ return (u64)((u128)a * b % p); }
static inline u64 addmod(u64 a, u64 b, u64 p){ u64 r = a + b; return r >= p ? r - p : r; }
static inline u64 submod(u64 a, u64 b, u64 p){ return a >= b ? a - b : a + p - b; }
static u64 powmod(u64 a, u64 e, u64 p){
    u64 r = 1; a %= p;
    while (e){ if (e & 1) r = mulmod(r, a, p); a = mulmod(a, a, p); e >>= 1; }
    return r;
}
static void ntt(vector<u64>& a, u64 p, bool invert){
    int n = (int)a.size();
    for (int i = 1, j = 0; i < n; i++){
        int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1){
        u64 wlen = powmod(G, (p - 1) / (u64)len, p);
        if (invert) wlen = powmod(wlen, p - 2, p);
        for (int i = 0; i < n; i += len){
            u64 w = 1;
            for (int k = 0; k < len / 2; k++){
                u64 u = a[i + k];
                u64 v = mulmod(a[i + k + len / 2], w, p);
                a[i + k] = addmod(u, v, p);
                a[i + k + len / 2] = submod(u, v, p);
                w = mulmod(w, wlen, p);
            }
        }
    }
    if (invert){ u64 invn = powmod((u64)n, p - 2, p); for (int i = 0; i < n; i++) a[i] = mulmod(a[i], invn, p); }
}
static vector<u64> to_L(const vector<u64>& a){
    vector<u64> out; u128 acc = 0;
    for (int i = (int)a.size() - 1; i >= 0; i--){
        acc = (acc << 64) | a[i];
        while (acc > MASKL){ out.push_back((u64)(acc & MASKL)); acc >>= L; }
    }
    while (acc > 0){ out.push_back((u64)(acc & MASKL)); acc >>= L; }
    return out;
}
static vector<u64> hex_to_limbs64(const string& s){
    vector<u64> r;
    for (size_t i = s.size(); i > 0; ){
        size_t a = (i >= 16) ? i - 16 : 0;
        string chunk = s.substr(a, i - a);
        r.push_back(strtoull(chunk.c_str(), nullptr, 16));
        if (i <= 16) break;
        i -= 16;
    }
    return r;
}

int main(int argc, char** argv){
    string path = argc > 1 ? argv[1] : "indbg/large_01.in";
    ifstream fin(path); string all((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
    istringstream iss(all);
    // mode: second token (cin>>) vs full-join
    string mode = argc > 2 ? argv[2] : "token";
    vector<string> toks; istringstream tiss(all); string tk; while (tiss >> tk) toks.push_back(tk);
    string A = toks[0];
    string B;
    if (mode == "token") B = toks.size() > 1 ? toks[1] : "";
    else { for (size_t i = 1; i < toks.size(); i++) B += toks[i]; }
    fprintf(stderr, "mode=%s A.len=%zu B.len=%zu\n", mode.c_str(), A.size(), B.size());

    // strip sign
    auto ss = [&](string& s)->bool{bool n=!s.empty()&&s[0]=='-'; if(n) s=s.substr(1); return n;};
    bool negA = ss(A), negB = ss(B);
    (void)negA; (void)negB;

    vector<u64> a64 = hex_to_limbs64(A);
    vector<u64> b64 = hex_to_limbs64(B);
    vector<u64> ca = to_L(a64), cb = to_L(b64);
    size_t na = ca.size(), nb = cb.size();
    int N = 1; while ((size_t)N < na + nb - 1) N <<= 1;
    fprintf(stderr, "na=%zu nb=%zu N=%d nc=%zu\n", na, nb, N, na+nb-1);
    ca.resize(N,0); cb.resize(N,0);
    fprintf(stderr, "DEBUG ca[0..3]=%llu %llu %llu %llu\n", (unsigned long long)ca[0],(unsigned long long)ca[1],(unsigned long long)ca[2],(unsigned long long)ca[3]);
    fprintf(stderr, "DEBUG cb[0..3]=%llu %llu %llu %llu\n", (unsigned long long)cb[0],(unsigned long long)cb[1],(unsigned long long)cb[2],(unsigned long long)cb[3]);
    { u128 t=0; for(size_t k=0;k<na;k++) for(size_t l=0;l<nb;l++) if(k+l==2) t+=(u128)ca[k]*cb[l]; fprintf(stderr,"DEBUG sum@2 (full)=%llu\n",(unsigned long long)t); }

    u64 P[3] = {P0,P1,P2};
    for (int pi = 0; pi < 3; pi++){
        u64 p = P[pi];
        vector<u64> fa(N,0), fb(N,0);
        for (int i=0;i<N;i++){ fa[i]=ca[i]%p; fb[i]=cb[i]%p; }
        ntt(fa,p,false); ntt(fb,p,false);
        for (int i=0;i<N;i++) fa[i]=mulmod(fa[i],fb[i],p);
        ntt(fa,p,true);
        // naive mod p
        vector<u64> nv(N,0);
        for (size_t i=0;i<na;i++) for (size_t j=0;j<nb;j++) if(i+j<N) nv[i+j]=(nv[i+j]+(u64)((u128)ca[i]*cb[j]%p))%p;
        size_t first_bad = N;
        for (size_t i=0;i<na+nb-1;i++) if (fa[i]!=nv[i]){ first_bad=i; break; }
        if (first_bad < N)
            fprintf(stderr, "PRIME p=%llu  DIFF@%zu ntt=%llu naive=%llu\n", (unsigned long long)p, (unsigned long long)first_bad, (unsigned long long)fa[first_bad], (unsigned long long)nv[first_bad]);
        else
            fprintf(stderr, "PRIME p=%llu  PASS (naive mod p match)\n", (unsigned long long)p);
        if (p == P2){
            fprintf(stderr, "   [p2 detail] fa[2]=%llu nv[2]=%llu fa[0]=%llu nv[0]=%llu\n",
                    (unsigned long long)fa[2], (unsigned long long)nv[2],
                    (unsigned long long)fa[0], (unsigned long long)nv[0]);
        }
    }
    // also full u128 naive vs CRT for first few coeffs
    vector<u128> naive(na+nb-1,0);
    for (size_t i=0;i<na;i++) for (size_t j=0;j<nb;j++) naive[i+j]+=(u128)ca[i]*cb[j];
    u64 inv01 = powmod(P0,P1-2,P1), inv012 = powmod(mulmod(P0,P1,P2),P2-2,P2);
    vector<u64> fa0(N,0),fb0(N,0),fa1(N,0),fb1(N,0),fa2(N,0),fb2(N,0);
    for(int i=0;i<N;i++){fa0[i]=ca[i]%P0;fb0[i]=cb[i]%P0;fa1[i]=ca[i]%P1;fb1[i]=cb[i]%P1;fa2[i]=ca[i]%P2;fb2[i]=cb[i]%P2;}
    ntt(fa0,P0,false);ntt(fb0,P0,false);ntt(fa1,P1,false);ntt(fb1,P1,false);ntt(fa2,P2,false);ntt(fb2,P2,false);
    for(int i=0;i<N;i++){fa0[i]=mulmod(fa0[i],fb0[i],P0);fa1[i]=mulmod(fa1[i],fb1[i],P1);fa2[i]=mulmod(fa2[i],fb2[i],P2);}
    ntt(fa0,P0,true);ntt(fa1,P1,true);ntt(fa2,P2,true);
    for (size_t i=0;i<na+nb-1;i++){
        u64 r0=fa0[i],r1=fa1[i],r2=fa2[i];
        u64 t1=mulmod(submod(r1,r0,P1),inv01,P1);
        u64 v=addmod(r0%P2,mulmod(P0,t1,P2),P2);
        u64 t2=mulmod(submod(r2,v,P2),inv012,P2);
        u128 val=(u128)r0+(u128)P0*t1+(u128)P0*P1*t2;
        if (val!=naive[i]){
            fprintf(stderr,"CRT DIFF@%zu\n",(unsigned long long)i);
            fprintf(stderr,"  r0=%llu r1=%llu r2=%llu\n",(unsigned long long)r0,(unsigned long long)r1,(unsigned long long)r2);
            fprintf(stderr,"  t1=%llu v=%llu t2=%llu\n",(unsigned long long)t1,(unsigned long long)v,(unsigned long long)t2);
            fprintf(stderr,"  val=%llu (lo32) naive=%llu (lo32) naive_full=%llu\n",(unsigned long long)(val&0xFFFFFFFFULL),(unsigned long long)(naive[i]&0xFFFFFFFFULL),(unsigned long long)naive[i]);
            fprintf(stderr,"  P0*t1=%llu  P0*P1*t2=%llu\n",(unsigned long long)((u128)P0*t1),(unsigned long long)((u128)P0*P1*t2));
            break;
        }
    }
    return 0;
}
