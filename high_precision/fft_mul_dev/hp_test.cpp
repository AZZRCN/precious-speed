// Validate: floor(a*b / B^k) == a1*b + floor(a0*b / B^k)  (limb-base B=2^64)
// where a1 = a[k..na), a0 = a[0..k). Tests shape A of invertappr:
//   na=n, nb=h, k=h  ->  top n of d*xh  (n+h limbs, drop bottom h).
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <random>

using u64 = uint64_t;
using u128 = unsigned __int128;

static void sb(const u64* a, int na, const u64* b, int nb, u64* c) {
    std::memset(c, 0, (size_t)(na + nb) * 8);
    for (int i = 0; i < nb; ++i) {
        u64 carry = 0, bi = b[i];
        for (int j = 0; j < na; ++j) {
            u128 t = (u128)a[j] * bi + c[i + j] + carry;
            c[i + j] = (u64)t;
            carry = (u64)(t >> 64);
        }
        c[i + na] = carry;
    }
}
// out[0..na+nb-k) = floor(a*b / B^k) via identity.
static void fdiv(const u64* a, int na, const u64* b, int nb, int k, u64* out) {
    int a0n = (na < k) ? na : k;
    int na1 = (na > k) ? na - k : 0;
    int a0b_sz = a0n + nb;
    u64* a0b = new u64[a0b_sz + 1]();
    sb(a, a0n, b, nb, a0b);
    int a1b_sz = na1 + nb;
    u64* a1b = new u64[a1b_sz + 1]();
    if (na1 > 0) sb(a + k, na1, b, nb, a1b);
    int outsz = na + nb - k;
    if (outsz < 0) outsz = 0;
    u128 carry = 0;
    for (int i = 0; i < outsz; ++i) {
        u128 x = (i < a1b_sz) ? a1b[i] : 0;
        u128 y = (i + k < a0b_sz) ? a0b[i + k] : 0;
        u128 s = x + y + carry;
        out[i] = (u64)s;
        carry = s >> 64;
    }
    delete[] a0b; delete[] a1b;
}

int main() {
    std::mt19937_64 rng(12345);
    auto rnd = [&](int bits) -> u64 {
        u64 v = 0; int b = 0;
        while (b < bits) { v |= (u64)rng() << b; b += 64; }
        if (bits < 64) v &= ((u64)1 << bits) - 1;
        return v;
    };
    int fails = 0, tests = 0;
    // generic random + shape A (na=n, nb=h, k=h)
    for (int iter = 0; iter < 4000; ++iter) {
        int n = 4 + (int)(rng() % 400);          // d limbs
        int h = (n >> 1) + 1;                     // xh limbs (invertappr convention)
        int na = n, nb = h, k = h;
        u64* a = new u64[na + 1]();
        u64* b = new u64[nb + 1]();
        for (int i = 0; i < na; ++i) a[i] = rnd(64);
        for (int i = 0; i < nb; ++i) b[i] = rnd(64);
        int full = na + nb;
        u64* fullp = new u64[full + 1]();
        sb(a, na, b, nb, fullp);
        int outsz = na + nb - k;
        u64* out = new u64[outsz + 1]();
        fdiv(a, na, b, nb, k, out);
        // reference: fullp[k .. na+nb)
        bool ok = true;
        for (int i = 0; i < outsz; ++i) {
            u64 ref = (k + i < full) ? fullp[k + i] : 0;
            if (out[i] != ref) { ok = false; break; }
        }
        if (!ok) {
            ++fails;
            if (fails <= 3) {
                printf("FAIL iter=%d n=%d h=%d k=%d outsz=%d\n", iter, n, h, k, outsz);
                for (int i = 0; i < outsz && i < 6; ++i)
                    printf("  out[%d]=%016llx ref=%016llx\n", i, (unsigned long long)out[i],
                           (unsigned long long)((k+i<full)?fullp[k+i]:0));
            }
        }
        ++tests;
        delete[] a; delete[] b; delete[] fullp; delete[] out;
    }
    // also random k not equal to nb
    for (int iter = 0; iter < 2000; ++iter) {
        int na = 2 + (int)(rng() % 60), nb = 2 + (int)(rng() % 60);
        int k = (int)(rng() % (na + nb + 1));
        u64* a = new u64[na + 1]();
        u64* b = new u64[nb + 1]();
        for (int i = 0; i < na; ++i) a[i] = rnd(64);
        for (int i = 0; i < nb; ++i) b[i] = rnd(64);
        int full = na + nb;
        u64* fullp = new u64[full + 1]();
        sb(a, na, b, nb, fullp);
        int outsz = na + nb - k; if (outsz < 0) outsz = 0;
        u64* out = new u64[outsz + 1]();
        fdiv(a, na, b, nb, k, out);
        bool ok = true;
        for (int i = 0; i < outsz; ++i) {
            u64 ref = (k + i < full) ? fullp[k + i] : 0;
            if (out[i] != ref) { ok = false; break; }
        }
        if (!ok) { ++fails; if (fails <= 6) printf("FAIL2 iter=%d na=%d nb=%d k=%d\n", iter, na, nb, k); }
        ++tests;
        delete[] a; delete[] b; delete[] fullp; delete[] out;
    }
    printf("tests=%d fails=%d\n", tests, fails);
    return fails ? 1 : 0;
}
