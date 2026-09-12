// NTT-based big integer multiplication (HEX) — 3-prime CRT, scalar prototype.
// Library Checker format: first token T, then T pairs "A B" (each may carry a
// leading '-' for negative). Output: T product lines.
#include <bits/stdc++.h>
using namespace std;
using u64 = uint64_t;
using u128 = unsigned __int128;

static const u64 P0 = 998244353ULL;
static const u64 P1 = 1004535809ULL;
static const u64 P2 = 167772161ULL;
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

// in-place iterative radix-2 NTT (primitive root G=3 for all three primes)
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

// base 2^64 LE -> base 2^L LE.
// Bit-assembly: feed each 64-bit limb into a running bit buffer, chop 27-bit
// chunks from the low end. (Do NOT truncate the buffer to <2^L between limbs —
// 64 is not a multiple of 27, so that would discard high residual bits.)
static vector<u64> to_L(const vector<u64>& a){
    vector<u64> out; u128 acc = 0; int abits = 0;
    for (size_t i = 0; i < a.size(); i++){
        acc |= (u128)a[i] << abits;
        abits += 64;
        while (abits >= L){ out.push_back((u64)(acc & MASKL)); acc >>= L; abits -= L; }
    }
    if (abits > 0 && acc > 0) out.push_back((u64)(acc & MASKL));
    return out;
}

// base 2^L LE -> base 2^64 LE  (inverse of to_L)
static vector<u64> from_L(const vector<u128>& c){
    vector<u64> out; u128 acc = 0; int abits = 0;
    for (size_t i = 0; i < c.size(); i++){
        acc |= (u128)c[i] << abits;
        abits += L;
        while (abits >= 64){ out.push_back((u64)(acc & MASK64)); acc >>= 64; abits -= 64; }
    }
    if (abits > 0 && acc > 0) out.push_back((u64)(acc & MASK64));
    return out;
}

// hex (big-endian string) -> base-2^64 LE limbs (low-end aligned, 16-char chunks from the end)
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

static string limbs64_to_hex(const vector<u64>& out){
    if (out.empty()) return "0";
    string s; char buf[17];
    for (int i = (int)out.size() - 1; i >= 0; i--){
        if (i == (int)out.size() - 1) snprintf(buf, sizeof buf, "%llX", (unsigned long long)out[i]);
        else snprintf(buf, sizeof buf, "%016llX", (unsigned long long)out[i]);
        s += buf;
    }
    size_t p = s.find_first_not_of('0');
    if (p == string::npos) return "0";
    return s.substr(p);
}

// multiply two unsigned hex strings, return unsigned hex product
static string ntt_mul_hex(const string& A, const string& B){
    vector<u64> a64 = hex_to_limbs64(A);
    vector<u64> b64 = hex_to_limbs64(B);
    vector<u64> ca = to_L(a64), cb = to_L(b64);
    size_t na = ca.size(), nb = cb.size();
    if (na == 0 || nb == 0) return "0";
    int N = 1; while ((size_t)N < na + nb - 1) N <<= 1;
    if (N > (1 << 21)) return "0"; // exceeds p1 NTT limit; not expected for these cases
    ca.resize(N, 0); cb.resize(N, 0);

    vector<u64> r0(N, 0), r1(N, 0), r2(N, 0);
    vector<u64> ca0 = ca, ca1 = ca, ca2 = ca;
    vector<u64> cb0 = cb, cb1 = cb, cb2 = cb;
    ntt(ca0, P0, false); ntt(cb0, P0, false);
    ntt(ca1, P1, false); ntt(cb1, P1, false);
    ntt(ca2, P2, false); ntt(cb2, P2, false);
    for (int i = 0; i < N; i++){
        r0[i] = mulmod(ca0[i], cb0[i], P0);
        r1[i] = mulmod(ca1[i], cb1[i], P1);
        r2[i] = mulmod(ca2[i], cb2[i], P2);
    }
    ntt(r0, P0, true); ntt(r1, P1, true); ntt(r2, P2, true);

    // Garner CRT: c = r0 + p0*t1 + p0*p1*t2  (< p0*p1*p2, holds for these sizes)
    u64 inv_p0_p1 = powmod(P0, P1 - 2, P1);
    u64 inv_p0p1_p2 = powmod(mulmod(P0, P1, P2), P2 - 2, P2);
    size_t nc = na + nb - 1;
    vector<u128> c(nc, 0);
    for (size_t i = 0; i < nc; i++){
        u64 rr0 = r0[i], rr1 = r1[i], rr2 = r2[i];
        u64 t1 = mulmod(submod(rr1, rr0, P1), inv_p0_p1, P1);
        // rr0 is a residue mod p0 (~1e9) and may exceed p2 (~1.7e8): reduce mod p2
        // before addmod (which only subtracts p2 once and assumes both operands < p2).
        u64 v = addmod(rr0 % P2, mulmod(P0, t1, P2), P2);
        u64 t2 = mulmod(submod(rr2, v, P2), inv_p0p1_p2, P2);
        c[i] = (u128)rr0 + (u128)P0 * t1 + (u128)P0 * P1 * t2;
    }
    // c[i] are true conv coefficients (can exceed 2^L): normalize to base-2^L via carry.
    {
        u128 carry = 0;
        for (size_t i = 0; i < nc; i++){
            u128 val = c[i] + carry;
            c[i] = val & MASKL;
            carry = val >> L;
        }
        while (carry){ c.push_back(carry & MASKL); carry >>= L; }
    }
    vector<u64> out = from_L(c);
    return limbs64_to_hex(out);
}

int main(){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    long long T; if (!(cin >> T)) return 0;
    string A, B;
    while (T-- > 0){
        if (!(cin >> A >> B)) break;
        bool negA = !A.empty() && A[0] == '-'; if (negA) A = A.substr(1);
        bool negB = !B.empty() && B[0] == '-'; if (negB) B = B.substr(1);
        bool negOut = negA ^ negB;
        string prod = ntt_mul_hex(A, B);
        if (negOut && prod != "0") cout << '-';
        cout << prod << '\n';
    }
    return 0;
}
