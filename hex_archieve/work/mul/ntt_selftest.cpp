// NTT self-test: find primitive roots, verify round-trip + 3-prime convolution vs naive.
#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t;
using u128 = unsigned __int128;
static inline u64 mulmod(u64 a, u64 b, u64 p){ return (u64)((u128)a * b % p); }
static u64 powmod(u64 a, u64 e, u64 p){
    u64 r = 1; a %= p;
    while (e){ if (e & 1) r = mulmod(r, a, p); a = mulmod(a, a, p); e >>= 1; }
    return r;
}
static void ntt(vector<u64>& a, u64 p, u64 g, bool invert){
    int n = (int)a.size();
    for (int i = 1, j = 0; i < n; i++){
        int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1){
        u64 wlen = powmod(g, (p - 1) / (u64)len, p);
        if (invert) wlen = powmod(wlen, p - 2, p);
        for (int i = 0; i < n; i += len){
            u64 w = 1;
            for (int k = 0; k < len / 2; k++){
                u64 u = a[i + k], v = mulmod(a[i + k + len / 2], w, p);
                a[i + k] = (u + v) % p;
                a[i + k + len / 2] = (u + p - v) % p;
                w = mulmod(w, wlen, p);
            }
        }
    }
    if (invert){ u64 invn = powmod((u64)n, p - 2, p); for (int i = 0; i < n; i++) a[i] = mulmod(a[i], invn, p); }
}
static u64 find_root(u64 p, const vector<u64>& fac){
    for (u64 g = 2; g < 100; g++){
        bool ok = true;
        for (u64 q : fac){ if (powmod(g, (p - 1) / q, p) == 1){ ok = false; break; } }
        if (ok) return g;
    }
    return 0;
}
static const int L = 27;
static const u64 MASKL = (1ULL << L) - 1;
static const u128 ONE64 = (u128)1 << 64;
static const u64 MASK64 = 0xFFFFFFFFFFFFFFFFULL;
static vector<u64> to_L(const vector<u64>& a){
    vector<u64> out; u128 acc = 0;
    for (int i = (int)a.size() - 1; i >= 0; i--){
        acc = (acc << 64) | a[i];
        while (acc > MASKL){ out.push_back((u64)(acc & MASKL)); acc >>= L; }
    }
    while (acc > 0){ out.push_back((u64)(acc & MASKL)); acc >>= L; }
    return out;
}
static vector<u64> from_L(const vector<u128>& c){
    vector<u64> out; u128 acc = 0;
    for (int i = (int)c.size() - 1; i >= 0; i--){
        acc = (acc << L) | c[i];
        while (acc >= ONE64){ out.push_back((u64)(acc & MASK64)); acc >>= 64; }
    }
    while (acc > 0){ out.push_back((u64)(acc & MASK64)); acc >>= 64; }
    return out;
}
int main(){
    mt19937_64 rng(12345);
    vector<pair<u64, vector<u64>>> pris = {
        {998244353ULL, {2,7,17}},
        {1004535809ULL, {2,479}},
        {167772161ULL, {2,5}},
    };
    // 1) find roots + round-trip
    int N = 1 << 16;
    for (auto& [p, fac] : pris){
        u64 g = find_root(p, fac);
        fprintf(stderr, "p=%llu root=%llu\n", (unsigned long long)p, (unsigned long long)g);
        vector<u64> a(N);
        for (int i = 0; i < N; i++) a[i] = rng() % p;
        vector<u64> b = a;
        ntt(a, p, g, false); ntt(a, p, g, true);
        bool rt = true;
        for (int i = 0; i < N; i++) if (a[i] != b[i]){ rt = false; break; }
        fprintf(stderr, "  roundtrip=%s\n", rt ? "PASS" : "FAIL");
    }
    // 2) 3-prime convolution vs naive
    u64 P0 = 998244353, P1 = 1004535809, P2 = 167772161;
    u64 g0 = find_root(P0, {2,7,17}), g1 = find_root(P1, {2,479}), g2 = find_root(P2, {2,5});
    const int L = 27; u64 MASKL = (1ULL << L) - 1;
    int na = 300, nb = 250;
    vector<u64> A(na), B(nb);
    for (int i = 0; i < na; i++) A[i] = rng() & MASKL;
    for (int i = 0; i < nb; i++) B[i] = rng() & MASKL;
    // naive conv in base 2^L
    vector<u128> naive(na + nb - 1, 0);
    for (int i = 0; i < na; i++) for (int j = 0; j < nb; j++) naive[i + j] += (u128)A[i] * B[j];
    // ntt conv
    int M = 1; while (M < na + nb - 1) M <<= 1;
    auto prep = [&](const vector<u64>& v, u64 p) {
        vector<u64> x(M, 0); for (size_t i = 0; i < v.size(); i++) x[i] = v[i] % p; return x;
    };
    vector<u64> a0 = prep(A, P0), a1 = prep(A, P1), a2 = prep(A, P2);
    vector<u64> b0 = prep(B, P0), b1 = prep(B, P1), b2 = prep(B, P2);
    ntt(a0, P0, g0, false); ntt(b0, P0, g0, false);
    ntt(a1, P1, g1, false); ntt(b1, P1, g1, false);
    ntt(a2, P2, g2, false); ntt(b2, P2, g2, false);
    vector<u64> r0(M), r1(M), r2(M);
    for (int i = 0; i < M; i++){ r0[i] = mulmod(a0[i], b0[i], P0); r1[i] = mulmod(a1[i], b1[i], P1); r2[i] = mulmod(a2[i], b2[i], P2); }
    ntt(r0, P0, g0, true); ntt(r1, P1, g1, true); ntt(r2, P2, g2, true);
    u64 inv_p0_p1 = powmod(P0, P1 - 2, P1);
    u64 inv_p0p1_p2 = powmod(mulmod(P0, P1, P2), P2 - 2, P2);
    int nc = na + nb - 1; bool conv_ok = true;
    for (int i = 0; i < nc; i++){
        u64 rr0 = r0[i], rr1 = r1[i], rr2 = r2[i];
        u64 t1 = mulmod((rr1 + P1 - rr0) % P1, inv_p0_p1, P1);
        u64 v = (rr0 + mulmod(P0, t1, P2)) % P2;
        u64 t2 = mulmod((rr2 + P2 - v) % P2, inv_p0p1_p2, P2);
        u128 val = (u128)rr0 + (u128)P0 * t1 + (u128)P0 * P1 * t2;
        if (val != naive[i]){ conv_ok = false; fprintf(stderr, "  conv mismatch i=%d got=%llu want=%llu\n", i, (unsigned long long)val, (unsigned long long)naive[i]); break; }
    }
    fprintf(stderr, "3-prime conv vs naive: %s\n", conv_ok ? "PASS" : "FAIL");

    auto trim = [](vector<u64>& v){ while (v.size() > 1 && v.back() == 0) v.pop_back(); };
    // Test A: to_L / from_L round-trip
    {
        vector<u64> orig(500);
        for (auto& x : orig) x = rng();
        orig.back() |= (1ULL << 63);
        vector<u64> Lv = to_L(orig);
        vector<u128> cL(Lv.size());
        for (size_t i = 0; i < Lv.size(); i++) cL[i] = Lv[i];
        vector<u64> back = from_L(cL);
        vector<u64> o2 = orig; trim(o2); trim(back);
        bool ok = (o2.size() == back.size());
        for (size_t i = 0; ok && i < o2.size(); i++) if (o2[i] != back[i]) ok = false;
        fprintf(stderr, "to_L/from_L roundtrip: %s\n", ok ? "PASS" : "FAIL");
    }
    // Test B: full multiply (to_L -> ntt conv -> from_L) vs schoolbook base 2^64
    {
        vector<u64> A2(200), B2(150);
        for (auto& x : A2) x = rng();
        for (auto& x : B2) x = rng();
        A2.back() |= (1ULL << 63); B2.back() |= (1ULL << 63);
        vector<u64> Lp = to_L(A2), Lq = to_L(B2);
        int M2 = 1; while (M2 < (int)(Lp.size() + Lq.size() - 1)) M2 <<= 1;
        auto prep2 = [&](const vector<u64>& v, u64 p){ vector<u64> x(M2,0); for (size_t i=0;i<v.size();i++) x[i]=v[i]%p; return x; };
        vector<u64> x0=prep2(Lp,P0),x1=prep2(Lp,P1),x2=prep2(Lp,P2);
        vector<u64> y0=prep2(Lq,P0),y1=prep2(Lq,P1),y2=prep2(Lq,P2);
        ntt(x0,P0,g0,false);ntt(y0,P0,g0,false);ntt(x1,P1,g1,false);ntt(y1,P1,g1,false);ntt(x2,P2,g2,false);ntt(y2,P2,g2,false);
        vector<u64> z0(M2),z1(M2),z2(M2);
        for (int i=0;i<M2;i++){z0[i]=mulmod(x0[i],y0[i],P0);z1[i]=mulmod(x1[i],y1[i],P1);z2[i]=mulmod(x2[i],y2[i],P2);}
        ntt(z0,P0,g0,true);ntt(z1,P1,g1,true);ntt(z2,P2,g2,true);
        int nc2=(int)Lp.size()+(int)Lq.size()-1;
        vector<u128> cL2(nc2);
        for (int i=0;i<nc2;i++){
            u64 t1=mulmod((z1[i]+P1-z0[i])%P1,inv_p0_p1,P1);
            u64 v=(z0[i]+mulmod(P0,t1,P2))%P2;
            u64 t2=mulmod((z2[i]+P2-v)%P2,inv_p0p1_p2,P2);
            cL2[i]=(u128)z0[i]+(u128)P0*t1+(u128)P0*P1*t2;
        }
        vector<u64> prod = from_L(cL2);
        // schoolbook
        vector<u128> sb(A2.size()+B2.size()-1,0);
        for (size_t i=0;i<A2.size();i++) for (size_t j=0;j<B2.size();j++) sb[i+j]+=(u128)A2[i]*B2[j];
        vector<u64> sb64; u128 carry=0;
        for (auto x:sb){ u128 v=x+carry; sb64.push_back((u64)(v&MASK64)); carry=v>>64; }
        while (carry){ sb64.push_back((u64)(carry&MASK64)); carry>>=64; }
        trim(prod); trim(sb64);
        bool ok=(prod.size()==sb64.size());
        size_t first=0; bool found=false;
        for (size_t i=0;i<max(prod.size(),sb64.size());i++){
            u64 a = i<prod.size()?prod[i]:0, b=i<sb64.size()?sb64[i]:0;
            if(a!=b){ ok=false; if(!found){first=i;found=true;}
                if(i<3||i==first) fprintf(stderr,"  diff i=%zu prod=%llx sb=%llx\n",i,(unsigned long long)a,(unsigned long long)b);
            }
        }
        fprintf(stderr, "full multiply (to_L->ntt->from_L) vs schoolbook: %s  prodsz=%zu sbsz=%zu\n", ok?"PASS":"FAIL", prod.size(), sb64.size());
    }
    return 0;
}
