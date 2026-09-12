/*
Submission #386671
ID	Date	Problem	Lang	User	Status	Time	Memory
386671	2026/7/20 21:28:15	

Addition of Hex Big Integers
	C++23	(Anonymous)	AC	14 ms	11.38 Mib
	Name	Status	Time	Memory
	example_00	AC	0 ms	0.61 Mib
	small_00	AC	14 ms	8.28 Mib
	medium_00	AC	6 ms	7.80 Mib
	medium_01	AC	5 ms	7.04 Mib
	medium_02	AC	5 ms	7.00 Mib
	large_00	AC	5 ms	8.04 Mib
	large_01	AC	6 ms	8.04 Mib
	large_02	AC	5 ms	8.05 Mib
	max_max_00	AC	6 ms	9.00 Mib
	max_max_01	AC	6 ms	7.98 Mib
	max_max_02	AC	6 ms	8.70 Mib
	max_max_03	AC	6 ms	9.20 Mib
	max_max_04	AC	6 ms	8.36 Mib
	max_max_05	AC	5 ms	8.29 Mib
	max_max_06	AC	6 ms	9.05 Mib
	max_max_07	AC	6 ms	9.03 Mib
	sum_zero_00	AC	6 ms	7.17 Mib
	large_small_00	AC	7 ms	11.38 Mib
	power_00	AC	5 ms	7.02 Mib
	power_01	AC	5 ms	6.53 Mib
	carry_chain_00	AC	4 ms	4.51 Mib
	carry_chain_01	AC	5 ms	6.04 Mib
	carry_chain_02	AC	5 ms	4.50 Mib
	carry_chain_03	AC	5 ms	6.66 Mib
*/
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <immintrin.h>

#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("sse4.1")

static constexpr int MAXN = 4000010;
static constexpr int MAXC = (MAXN >> 4) + 4;

alignas(64) static char inbuf[MAXN];
alignas(64) static char outbuf[MAXN];
static uint64_t ca[MAXC], cb[MAXC], cr[MAXC];

static const char HEX[] = "0123456789ABCDEF";

// ============ SSE Hex Conversion ============

// 16 hex ASCII chars → uint64_t (SSE4.1, ~5 instructions)
static inline uint64_t hex16_load(const char* s) {
    __m128i input = _mm_loadu_si128((const __m128i*)s);
    __m128i sub0  = _mm_sub_epi8(input, _mm_set1_epi8('0'));
    __m128i suba  = _mm_sub_epi8(_mm_or_si128(input, _mm_set1_epi8(0x20)),
                                  _mm_set1_epi8('a' - 10));
    __m128i mask  = _mm_cmpgt_epi8(sub0, _mm_set1_epi8(9));
    __m128i nib   = _mm_or_si128(_mm_andnot_si128(mask, sub0),
                                  _mm_and_si128(mask, suba));
    __m128i bytes = _mm_maddubs_epi16(nib, _mm_set1_epi16(0x0110));
    bytes = _mm_packus_epi16(bytes, bytes);
    return __builtin_bswap64((uint64_t)_mm_cvtsi128_si64(bytes));
}

// uint64_t → 16 hex ASCII chars (SSE, ~6 instructions + 1 store)
static inline void hex16_store(uint64_t val, char* out) {
    val = __builtin_bswap64(val);
    __m128i v    = _mm_cvtsi64_si128((long long)val);
    __m128i hi   = _mm_and_si128(_mm_srli_epi16(v, 4), _mm_set1_epi8(0x0F));
    __m128i lo   = _mm_and_si128(v, _mm_set1_epi8(0x0F));
    __m128i nib  = _mm_unpacklo_epi8(hi, lo);
    __m128i gt9  = _mm_cmpgt_epi8(nib, _mm_set1_epi8(9));
    __m128i asc  = _mm_add_epi8(nib, _mm_set1_epi8('0'));
    __m128i alp  = _mm_add_epi8(nib, _mm_set1_epi8('A' - 10));
    _mm_storeu_si128((__m128i*)out,
                     _mm_or_si128(_mm_andnot_si128(gt9, asc),
                                  _mm_and_si128(gt9, alp)));
}

// Scalar: <16 hex chars → uint64_t (for MSB chunk only)
static inline uint64_t hex_short_load(const char* s, int len) {
    uint64_t v = 0;
    for (int i = 0; i < len; i++) {
        int d = s[i] - '0';
        if (d > 9) d = (s[i] | 0x20) - 'a' + 10;
        v = (v << 4) | (uint64_t)d;
    }
    return v;
}

// Scalar: uint64_t → hex string, no leading zeros (for MSB chunk)
static inline int hex_var_store(uint64_t v, char* out) {
    if (__builtin_expect(v == 0, 0)) { out[0] = '0'; return 1; }
    char tmp[16];
    int len = 0;
    while (v) { tmp[len++] = HEX[v & 15]; v >>= 4; }
    for (int i = 0; i < len; i++) out[i] = tmp[len - 1 - i];
    return len;
}

// ============ Main ============

int main() {
    int total = fread(inbuf, 1, MAXN - 1, stdin);
    inbuf[total] = '\0';

    char* p = inbuf;
    int T = 0;
    while (*p >= '0' && *p <= '9') T = T * 10 + (*p++ - '0');
    while (*p <= ' ') p++;

    int opos = 0;

    for (int t = 0; t < T; t++) {
        // ---- Parse A: inbuf → uint64_t chunks (zero-copy) ----
        int sa = 1;
        if (*p == '-') { sa = -1; p++; }
        const char* a_start = p;
        while (*p > ' ') p++;
        int la = (int)(p - a_start);
        while (*p <= ' ' && *p) p++;

        int nca = (la + 15) >> 4;
        {
            int pos = la;
            for (int c = 0; c < nca; c++) {
                int clen = pos >= 16 ? 16 : pos;
                pos -= clen;
                ca[c] = (clen == 16) ? hex16_load(a_start + pos)
                                     : hex_short_load(a_start + pos, clen);
            }
        }

        // ---- Parse B: inbuf → uint64_t chunks (zero-copy) ----
        int sb = 1;
        if (*p == '-') { sb = -1; p++; }
        const char* b_start = p;
        while (*p > ' ') p++;
        int lb = (int)(p - b_start);
        while (*p <= ' ' && *p) p++;

        int ncb = (lb + 15) >> 4;
        {
            int pos = lb;
            for (int c = 0; c < ncb; c++) {
                int clen = pos >= 16 ? 16 : pos;
                pos -= clen;
                cb[c] = (clen == 16) ? hex16_load(b_start + pos)
                                     : hex_short_load(b_start + pos, clen);
            }
        }

        // ---- Arithmetic on uint64_t chunks ----
        int rsign, ncr;

        if (sa == sb) {
            // ADD magnitudes
            rsign = sa;
            int minc = nca < ncb ? nca : ncb;
            int maxc = nca > ncb ? nca : ncb;

            uint64_t carry = 0;
            int i = 0;
            for (; i < minc; i++) {
                __uint128_t sum = (__uint128_t)ca[i] + cb[i] + carry;
                cr[i] = (uint64_t)sum;
                carry = (uint64_t)(sum >> 64);
            }
            const uint64_t* longer = (nca >= ncb) ? ca : cb;
            for (; i < maxc; i++) {
                __uint128_t sum = (__uint128_t)longer[i] + carry;
                cr[i] = (uint64_t)sum;
                carry = (uint64_t)(sum >> 64);
            }
            if (carry) cr[maxc++] = carry;
            ncr = maxc;
        } else {
            // SUBTRACT: compare magnitudes from MSB chunk
            int cmp = 0;
            int maxc = nca > ncb ? nca : ncb;
            for (int i = maxc - 1; i >= 0; i--) {
                uint64_t ai = (i < nca) ? ca[i] : 0;
                uint64_t bi = (i < ncb) ? cb[i] : 0;
                if (ai != bi) { cmp = (ai > bi) ? 1 : -1; break; }
            }

            if (__builtin_expect(cmp == 0, 0)) {
                outbuf[opos++] = '0';
                outbuf[opos++] = '\n';
                continue;
            }

            const uint64_t* big   = (cmp > 0) ? ca : cb;
            const uint64_t* small = (cmp > 0) ? cb : ca;
            int nbig   = (cmp > 0) ? nca : ncb;
            int nsmall = (cmp > 0) ? ncb : nca;
            rsign = (cmp > 0) ? sa : sb;

            uint64_t borrow = 0;
            int i = 0;
            for (; i < nsmall; i++) {
                uint64_t ai = big[i], bi = small[i];
                uint64_t sub = bi + borrow;
                cr[i] = ai - sub;
                borrow = (ai < sub) | (sub < bi);
            }
            for (; i < nbig; i++) {
                uint64_t ai = big[i];
                cr[i] = ai - borrow;
                borrow = (ai == 0) & borrow;
            }
            ncr = nbig;
        }

        // Trim leading zero chunks
        while (ncr > 1 && cr[ncr - 1] == 0) ncr--;

        // ---- Output (SSE vectorized) ----
        if (rsign < 0 && !(ncr == 1 && cr[0] == 0))
            outbuf[opos++] = '-';

        // MSB chunk: variable-length scalar
        opos += hex_var_store(cr[ncr - 1], outbuf + opos);

        // Remaining chunks: 16 hex chars each via SSE store
        for (int c = ncr - 2; c >= 0; c--) {
            hex16_store(cr[c], outbuf + opos);
            opos += 16;
        }

        outbuf[opos++] = '\n';
    }

    fwrite(outbuf, 1, opos, stdout);
    return 0;
}