// Karatsuba multiplication prototype
// Validates correctness and benchmarks against basicMul/FFT for 64-2048 limbs.
// BASE=10^4, Limb=uint16_t, same as mul.cpp
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>

using Limb = uint16_t;
static constexpr Limb BASE = 10000;
static constexpr Limb BASE_DIGIT = 4;
using Limb2 = uint32_t;
static constexpr uint64_t BASE8 = uint64_t(BASE) * BASE; // 10^8

// Basic O(n^2) multiplication with base-10^8 packing (same as mul.cpp basicMul)
void basicMul(const Limb* in1, size_t n1, const Limb* in2, size_t n2, Limb* out, size_t out_size) {
    size_t cn1 = (n1 + 1) / 2, cn2 = (n2 + 1) / 2;
    size_t cout_len = cn1 + cn2;
    std::vector<uint32_t> c1(cn1), c2(cn2);
    std::vector<uint64_t> cbuf(cout_len, 0);

    for (size_t i = 0; i < cn1; i++) {
        uint32_t v = in1[2 * i];
        if (2 * i + 1 < n1) v += uint32_t(in1[2 * i + 1]) * BASE;
        c1[i] = v;
    }
    for (size_t i = 0; i < cn2; i++) {
        uint32_t v = in2[2 * i];
        if (2 * i + 1 < n2) v += uint32_t(in2[2 * i + 1]) * BASE;
        c2[i] = v;
    }
    for (size_t i = 0; i < cn1; i++) {
        uint64_t carry = 0;
        uint64_t x = c1[i];
        for (size_t j = 0; j < cn2; j++) {
            uint64_t prod = uint64_t(c2[j]) * x + carry + cbuf[i + j];
            cbuf[i + j] = prod % BASE8;
            carry = prod / BASE8;
        }
        cbuf[i + cn2] = carry;
    }
    size_t out_len = n1 + n2;
    for (size_t i = 0; i < cout_len; i++) {
        uint64_t val = cbuf[i];
        size_t lo = 2 * i, hi = 2 * i + 1;
        if (lo < out_len && lo < out_size) out[lo] = Limb(val % BASE);
        if (hi < out_len && hi < out_size) out[hi] = Limb(val / BASE);
    }
    for (size_t i = out_len; i < out_size; i++) out[i] = 0;
}

// Add: out = a + b, returns carry. out has at least max(na,nb) space.
size_t addLimb(const Limb* a, size_t na, const Limb* b, size_t nb, Limb* out) {
    if (na < nb) { std::swap(a, b); std::swap(na, nb); }
    Limb2 carry = 0;
    for (size_t i = 0; i < nb; i++) {
        Limb2 s = Limb2(a[i]) + b[i] + carry;
        out[i] = s % BASE;
        carry = s / BASE;
    }
    for (size_t i = nb; i < na; i++) {
        Limb2 s = Limb2(a[i]) + carry;
        out[i] = s % BASE;
        carry = s / BASE;
    }
    if (carry) out[na++] = Limb(carry);
    return na;
}

// Sub: out = a - b, a >= b. returns length.
size_t subLimb(const Limb* a, size_t na, const Limb* b, size_t nb, Limb* out) {
    int32_t borrow = 0;
    for (size_t i = 0; i < nb; i++) {
        int32_t d = int32_t(a[i]) - b[i] - borrow;
        if (d < 0) { d += BASE; borrow = 1; } else borrow = 0;
        out[i] = Limb(d);
    }
    for (size_t i = nb; i < na; i++) {
        int32_t d = int32_t(a[i]) - borrow;
        if (d < 0) { d += BASE; borrow = 1; } else borrow = 0;
        out[i] = Limb(d);
    }
    while (na > 0 && out[na - 1] == 0) na--;
    return na;
}

// Karatsuba threshold: below this, use basicMul
static constexpr size_t KARATSUBA_THRESHOLD = 64;

// Recursive Karatsuba. Uses local vectors for intermediate sums to avoid
// scratch buffer corruption across recursive calls.
void karatsubaRec(const Limb* a, size_t na, const Limb* b, size_t nb, Limb* out) {
    if (na < nb) { std::swap(a, b); std::swap(na, nb); }
    if (nb <= KARATSUBA_THRESHOLD || na < 2) {
        basicMul(a, na, b, nb, out, na + nb);
        return;
    }
    size_t m = (na + 1) / 2;
    const Limb* a_lo = a;
    const Limb* a_hi = a + m;
    size_t na_lo = std::min(m, na);
    size_t na_hi = na - m;

    // Zero out output
    std::fill(out, out + na + nb, 0);

    // Check if b is too short to split (unbalanced case)
    if (nb <= m) {
        // b_hi is zero. a*b = a_lo*b + a_hi*b*B^m
        // z0 = a_lo * b -> out[0..]
        karatsubaRec(a_lo, na_lo, b, nb, out);
        // z1 = a_hi * b -> temp buffer
        size_t z1_size = na_hi + nb;
        std::vector<Limb> z1_temp(z1_size, 0);
        karatsubaRec(a_hi, na_hi, b, nb, z1_temp.data());
        // out[m..] += z1_temp
        Limb2 carry = 0;
        for (size_t i = 0; i < z1_size; i++) {
            Limb2 s = Limb2(out[m + i]) + z1_temp[i] + carry;
            out[m + i] = s % BASE;
            carry = s / BASE;
        }
        size_t wp = m + z1_size;
        while (carry && wp < na + nb) {
            Limb2 s = Limb2(out[wp]) + carry;
            out[wp] = s % BASE;
            carry = s / BASE;
            wp++;
        }
        return;
    }

    // Balanced case: both a and b can be split
    const Limb* b_lo = b;
    const Limb* b_hi = b + m;
    size_t nb_lo = m;
    size_t nb_hi = nb - m;

    // z0 = a_lo * b_lo -> out[0..]
    karatsubaRec(a_lo, na_lo, b_lo, nb_lo, out);
    // z2 = a_hi * b_hi -> out[2m..]
    karatsubaRec(a_hi, na_hi, b_hi, nb_hi, out + 2 * m);

    // Compute sums (local vectors, safe from recursive overwrites)
    size_t sum_a_size = std::max(na_lo, na_hi) + 1;
    size_t sum_b_size = std::max(nb_lo, nb_hi) + 1;
    std::vector<Limb> sum_a(sum_a_size), sum_b(sum_b_size);
    size_t sa_len = addLimb(a_lo, na_lo, a_hi, na_hi, sum_a.data());
    size_t sb_len = addLimb(b_lo, nb_lo, b_hi, nb_hi, sum_b.data());

    // z1_temp = (a_lo+a_hi) * (b_lo+b_hi)
    size_t z1_size = sa_len + sb_len;
    std::vector<Limb> z1_temp(z1_size, 0);
    karatsubaRec(sum_a.data(), sa_len, sum_b.data(), sb_len, z1_temp.data());

    // Save copies of z0 and z2 before modifying out
    size_t z0_len = na_lo + nb_lo;
    size_t z2_len = na_hi + nb_hi;
    std::vector<Limb> z0_copy(z0_len), z2_copy(z2_len);
    std::copy_n(out, z0_len, z0_copy.data());
    std::copy_n(out + 2 * m, z2_len, z2_copy.data());

    // z1_temp -= z0, z1_temp -= z2
    size_t z1_len = z1_size;
    z1_len = subLimb(z1_temp.data(), z1_len, z0_copy.data(), z0_len, z1_temp.data());
    z1_len = subLimb(z1_temp.data(), z1_len, z2_copy.data(), z2_len, z1_temp.data());

    // out[m..] += z1_temp
    Limb2 carry = 0;
    for (size_t i = 0; i < z1_len; i++) {
        Limb2 s = Limb2(out[m + i]) + z1_temp[i] + carry;
        out[m + i] = s % BASE;
        carry = s / BASE;
    }
    size_t wp = m + z1_len;
    while (carry && wp < na + nb) {
        Limb2 s = Limb2(out[wp]) + carry;
        out[wp] = s % BASE;
        carry = s / BASE;
        wp++;
    }
}

// Public Karatsuba entry
void karatsubaMul(const Limb* a, size_t na, const Limb* b, size_t nb, Limb* out, size_t out_size) {
    if (na == 0 || nb == 0) {
        for (size_t i = 0; i < out_size; i++) out[i] = 0;
        return;
    }
    karatsubaRec(a, na, b, nb, out);
    for (size_t i = na + nb; i < out_size; i++) out[i] = 0;
}

// Generate random limb array
std::vector<Limb> randLimbs(size_t n, std::mt19937& rng) {
    std::vector<Limb> v(n);
    for (size_t i = 0; i < n; i++) v[i] = rng() % BASE;
    return v;
}

// Compare two limb arrays
bool compareLimbs(const std::vector<Limb>& a, const std::vector<Limb>& b) {
    size_t la = a.size(), lb = b.size();
    while (la > 0 && a[la-1] == 0) la--;
    while (lb > 0 && b[lb-1] == 0) lb--;
    if (la != lb) return false;
    for (size_t i = 0; i < la; i++) if (a[i] != b[i]) return false;
    return true;
}

int main() {
    printf("=== Karatsuba Correctness Test ===\n");
    std::mt19937 rng(42);
    bool all_pass = true;

    struct TestCase { size_t na, nb; };
    TestCase cases[] = {
        {1, 1}, {5, 5}, {32, 32}, {64, 64}, {65, 65}, {100, 100},
        {128, 128}, {200, 150}, {256, 256}, {500, 500}, {1000, 1000},
        {100, 50}, {300, 100}, {1024, 1024}, {2048, 2048},
    };

    for (auto& tc : cases) {
        auto a = randLimbs(tc.na, rng);
        auto b = randLimbs(tc.nb, rng);
        std::vector<Limb> out_basic(tc.na + tc.nb, 0);
        std::vector<Limb> out_kara(tc.na + tc.nb, 0);
        basicMul(a.data(), tc.na, b.data(), tc.nb, out_basic.data(), tc.na + tc.nb);
        printf("  [debug] basicMul done for na=%zu nb=%zu\n", tc.na, tc.nb); fflush(stdout);
        karatsubaMul(a.data(), tc.na, b.data(), tc.nb, out_kara.data(), tc.na + tc.nb);
        printf("  [debug] karatsuba done\n"); fflush(stdout);
        bool ok = compareLimbs(out_basic, out_kara);
        printf("  na=%4zu nb=%4zu: %s\n", tc.na, tc.nb, ok ? "PASS" : "FAIL");
        if (!ok) all_pass = false;
    }

    if (!all_pass) { printf("FAILED\n"); return 1; }
    printf("All PASS\n\n");

    printf("=== Karatsuba vs basicMul Benchmark ===\n");
    printf("  %8s %8s %10s %10s %8s\n", "na", "nb", "basic(ms)", "kara(ms)", "ratio");
    fflush(stdout);

    TestCase bench[] = {
        {32, 32}, {48, 48}, {64, 64}, {96, 96}, {128, 128}, {192, 192},
        {256, 256}, {384, 384}, {512, 512}, {768, 768}, {1024, 1024},
        {1536, 1536}, {2048, 2048}, {100, 100}, {200, 200}, {500, 500},
    };
    for (auto& tc : bench) {
        auto a = randLimbs(tc.na, rng);
        auto b = randLimbs(tc.nb, rng);
        std::vector<Limb> out(tc.na + tc.nb, 0);
        int R = 100;
        // basic
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < R; r++) basicMul(a.data(), tc.na, b.data(), tc.nb, out.data(), tc.na + tc.nb);
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_basic = std::chrono::duration<double, std::milli>(t1 - t0).count() / R;
        // kara
        t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < R; r++) karatsubaMul(a.data(), tc.na, b.data(), tc.nb, out.data(), tc.na + tc.nb);
        t1 = std::chrono::high_resolution_clock::now();
        double t_kara = std::chrono::duration<double, std::milli>(t1 - t0).count() / R;
        double ratio = t_kara / t_basic;
        printf("  %8zu %8zu %10.4f %10.4f %8.3f\n", tc.na, tc.nb, t_basic, t_kara, ratio);
        fflush(stdout);
    }
    return 0;
}
