// HEX addition v1 — 64-bit limbs (16 hex digits/limb), SSE4.1 parse/store.
// I/O: T then T lines "A B" (hex, optional '-'); output A+B per line.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <immintrin.h>

#pragma GCC target("sse4.1")

static constexpr int MAXN = 8000010;
static constexpr int MAXC = (MAXN >> 4) + 4;

alignas(64) static char inbuf[MAXN];
alignas(64) static char outbuf[MAXN];
static uint64_t A[MAXC], B[MAXC], R[MAXC];

static const char HEX[] = "0123456789ABCDEF";

static inline int hn(char c) {
    int d = c - '0';
    if (d > 9) d = (c | 0x20) - 'a' + 10;
    return d & 0xF;
}

// 16 hex ASCII chars (little-endian nibbles, high char = MSB) -> uint64_t
static inline uint64_t hex16_load(const char* s) {
    __m128i input = _mm_loadu_si128((const __m128i*)s);
    __m128i sub0 = _mm_sub_epi8(input, _mm_set1_epi8('0'));
    __m128i subA = _mm_sub_epi8(_mm_or_si128(input, _mm_set1_epi8(0x20)),
                                _mm_set1_epi8('a' - 10));
    __m128i mask = _mm_cmpgt_epi8(sub0, _mm_set1_epi8(9));
    __m128i nib = _mm_or_si128(_mm_andnot_si128(mask, sub0),
                               _mm_and_si128(mask, subA));
    __m128i bytes = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    bytes = _mm_packus_epi16(bytes, bytes);
    return __builtin_bswap64((uint64_t)_mm_cvtsi128_si64(bytes));
}

static inline void hex16_store(uint64_t val, char* out) {
    val = __builtin_bswap64(val);
    __m128i v = _mm_cvtsi64_si128((long long)val);
    __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
    __m128i lo = _mm_and_si128(v, _mm_set1_epi8(0x0F));
    __m128i nib = _mm_unpacklo_epi8(hi, lo);
    __m128i gt9 = _mm_cmpgt_epi8(nib, _mm_set1_epi8(9));
    __m128i asc = _mm_add_epi8(nib, _mm_set1_epi8('0'));
    __m128i alp = _mm_add_epi8(nib, _mm_set1_epi8('A' - 10));
    _mm_storeu_si128((__m128i*)out,
                     _mm_or_si128(_mm_andnot_si128(gt9, asc),
                                  _mm_and_si128(gt9, alp)));
}

// parse hex [s,s+n) -> little-endian limbs; returns limb count
static int parse(const char* s, int n, uint64_t* out) {
    int nc = (n + 15) >> 4;
    int pos = n;
    for (int c = 0; c < nc; c++) {
        int clen = pos >= 16 ? 16 : pos;
        pos -= clen;
        if (clen == 16) out[c] = hex16_load(s + pos);
        else {
            uint64_t v = 0;
            const char* p = s + pos;
            for (int i = 0; i < clen; i++) v = (v << 4) | (uint64_t)hn(p[i]);
            out[c] = v;
        }
    }
    return nc;
}

// compare magnitudes |A| vs |B| from top limb
static int mag_cmp(const uint64_t* a, int na, const uint64_t* b, int nb) {
    int maxc = na > nb ? na : nb;
    for (int i = maxc - 1; i >= 0; i--) {
        uint64_t ai = i < na ? a[i] : 0;
        uint64_t bi = i < nb ? b[i] : 0;
        if (ai != bi) return ai > bi ? 1 : -1;
    }
    return 0;
}

int main() {
    int total = (int)fread(inbuf, 1, MAXN - 1, stdin);
    inbuf[total] = '\0';
    char* p = inbuf;
    int T = 0;
    while (*p >= '0' && *p <= '9') T = T * 10 + (*p++ - '0');
    while (*p <= ' ') p++;

    int opos = 0;
    for (int t = 0; t < T; t++) {
        // parse A
        int sa = 1;
        if (*p == '-') { sa = -1; p++; }
        const char* a0 = p;
        while (*p > ' ') p++;
        int la = (int)(p - a0);
        while (*p <= ' ' && *p) p++;
        // parse B
        int sb = 1;
        if (*p == '-') { sb = -1; p++; }
        const char* b0 = p;
        while (*p > ' ') p++;
        int lb = (int)(p - b0);
        while (*p <= ' ' && *p) p++;

        int nca = parse(a0, la, A);
        int ncb = parse(b0, lb, B);

        int rsign, ncr;
        if (sa == sb) {
            int minc = nca < ncb ? nca : ncb;
            int maxc = nca > ncb ? nca : ncb;
            uint64_t carry = 0;
            int i = 0;
            for (; i < minc; i++) {
                __uint128_t s = (__uint128_t)A[i] + B[i] + carry;
                R[i] = (uint64_t)s;
                carry = (uint64_t)(s >> 64);
            }
            const uint64_t* L = (nca >= ncb) ? A : B;
            for (; i < maxc; i++) {
                __uint128_t s = (__uint128_t)L[i] + carry;
                R[i] = (uint64_t)s;
                carry = (uint64_t)(s >> 64);
            }
            if (carry) R[maxc++] = carry;
            ncr = maxc;
            rsign = sa;
        } else {
            int cmp = mag_cmp(A, nca, B, ncb);
            if (cmp == 0) {
                outbuf[opos++] = '0';
                outbuf[opos++] = '\n';
                continue;
            }
            const uint64_t* big = cmp > 0 ? A : B;
            const uint64_t* small = cmp > 0 ? B : A;
            int nbig = cmp > 0 ? nca : ncb;
            int nsmall = cmp > 0 ? ncb : nca;
            rsign = cmp > 0 ? sa : sb;
            uint64_t borrow = 0;
            int i = 0;
            for (; i < nsmall; i++) {
                uint64_t ai = big[i], bi = small[i];
                uint64_t sub = bi + borrow;
                R[i] = ai - sub;
                borrow = (ai < sub) | (sub < bi);
            }
            for (; i < nbig; i++) {
                uint64_t ai = big[i];
                R[i] = ai - borrow;
                borrow = (ai == 0) & borrow;
            }
            ncr = nbig;
        }

        while (ncr > 1 && R[ncr - 1] == 0) ncr--;

        if (rsign < 0 && !(ncr == 1 && R[0] == 0)) outbuf[opos++] = '-';

        // MSB chunk variable-length
        uint64_t top = R[ncr - 1];
        int d = 0; uint64_t tmp = top;
        while (tmp) { d++; tmp >>= 4; }
        if (d == 0) d = 1;
        for (int i = d - 1; i >= 0; i--) outbuf[opos++] = HEX[(top >> (4 * i)) & 15];
        // remaining chunks fixed 16
        for (int c = ncr - 2; c >= 0; c--) {
            hex16_store(R[c], outbuf + opos);
            opos += 16;
        }
        outbuf[opos++] = '\n';
    }
    fwrite(outbuf, 1, opos, stdout);
    return 0;
}
