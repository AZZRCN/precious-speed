// fast_io.cpp - Independent fast I/O benchmark for ADD small_00
// Goal: Develop theoretically fastest integer read/write under O2
// Compile: g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE fast_io.cpp -o fast_io
// Test:   ./fast_io < input.txt > output.txt
// Version select: -DPARSE_VER=1/2/3, -DWRITE_VER=1/2
//   PARSE_VER=1: SWAR 4-byte grouped (baseline, from moptm)
//   PARSE_VER=2: 8-byte first read + SWAR parse8
//   PARSE_VER=3: builtin SIMD (pmaddubsw + pmaddwd)
//   WRITE_VER=1: 10000-base + outTable (baseline)
//   WRITE_VER=2: Barrett reduction + SSE2 store

#pragma GCC target("avx2,bmi,bmi2,popcnt,lzcnt")
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PARSE_VER
#define PARSE_VER 1
#endif
#ifndef WRITE_VER
#define WRITE_VER 1
#endif

// === Input (mmap zero-copy) ===
static const char* iBase = nullptr;
static const char* iCursor = nullptr;
static const char* iEnd = nullptr;

// === Output (oBuffer) ===
static char* oBuffer = nullptr;
static char* oCursor = nullptr;
static constexpr size_t OBUF_SIZE = 1 << 22; // 4MB

// === outTable[0..9999] = 4-byte packed ASCII (zero-padded) ===
static uint32_t outTable[10000];

static void initOutTable() {
    for (uint32_t i = 0, *p = outTable, a = '0'; a <= '9'; ++a)
        for (uint32_t b = '0'; b <= '9'; ++b)
            for (uint32_t c = '0'; c <= '9'; ++c)
                for (uint32_t d = '0'; d <= '9'; ++d)
                    *p++ = a | (b << 8) | (c << 16) | (d << 24);
}

static void initInput() {
    struct stat st;
    if (fstat(0, &st) < 0) { perror("fstat"); exit(1); }
    size_t sz = st.st_size;
    void* m = mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, 0, 0);
    if (m == MAP_FAILED) { perror("mmap"); exit(1); }
    // Hint kernel: sequential access + will need (prefault/preread)
    madvise(m, sz, MADV_SEQUENTIAL | MADV_WILLNEED);
    iBase = iCursor = (const char*)m;
    iEnd = iBase + sz;
    // Aligned output buffer via mmap (THP-friendly)
    oBuffer = (char*)mmap(nullptr, OBUF_SIZE, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (oBuffer == MAP_FAILED) { perror("oBuffer mmap"); exit(1); }
    madvise(oBuffer, OBUF_SIZE, MADV_HUGEPAGE);
    oCursor = oBuffer;
}

static void flushOutput() {
    if (oCursor > oBuffer) {
        fwrite(oBuffer, 1, oCursor - oBuffer, stdout);
        oCursor = oBuffer;
    }
}

// === SWAR 4-byte ASCII digits -> uint32 (0-9999) ===
static inline uint32_t parse4SWAR(const char* s) {
    uint32_t v;
    std::memcpy(&v, s, 4);
    v = (v & 0x0F0F0F0Fu) * 2561u;
    v = ((v >> 8) & 0x00FF00FFu) * 6553601u;
    return (v >> 16) & 0xFFFFu;
}

// === SWAR 8-byte ASCII digits -> uint64 (0-99999999) ===
// Daniel Lemire technique: 3 multiplies
static inline uint64_t parse8SWAR(const char* s) {
    uint64_t v;
    std::memcpy(&v, s, 8);
    v = (v & 0x0F0F0F0F0F0F0F0FULL) * 2561ULL;
    v = ((v >> 8) & 0x00FF00FF00FF00FFULL) * 6553601ULL;
    v = ((v >> 16) & 0x0000FFFF0000FFFFULL) * 42949672960001ULL;
    return v >> 32;
}

// === Check if all 8 bytes are ASCII digits (0x30-0x39) ===
// Uses nibble-based check (no inter-byte borrow issues)
// high nibble must == 3, low nibble must <= 9
static inline bool allDigits8(uint64_t v) {
    uint64_t high = v & 0xF0F0F0F0F0F0F0F0ULL;
    if (high != 0x3030303030303030ULL) return false;
    uint64_t low = v & 0x0F0F0F0F0F0F0F0FULL;
    // low + 6: if any low nibble > 9, carries into bit 4 (high nibble of same byte)
    if ((low + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL) return false;
    return true;
}

// === SWAR find first byte <= 0x20 (whitespace/delimiter) in 8 bytes ===
// Returns bitmask: bit i*8 set if byte i is <= 0x20
// Used for long-token boundary detection (same as moptm swarTokenLen)
static inline uint64_t delimiterMask8(uint64_t v) {
    return (~v) & (v - 0x2121212121212121ULL) & 0x8080808080808080ULL;
}

//=================================================================
// PARSE VER 1: SWAR 4-byte grouped (baseline from moptm)
//=================================================================
#if PARSE_VER == 1

static inline int64_t parsePositive(const char* s, size_t& len) {
    int64_t v = 0;
    size_t i = 0;
    // 4-byte grouped: max 4 groups (16 digits)
    while (i + 4 <= 16) {
        uint32_t d;
        std::memcpy(&d, s + i, 4);
        if ((d & 0xF0F0F0F0u) != 0x30303030u) break;
        uint32_t low = d & 0x0F0F0F0Fu;
        if ((low + 0x06060606u) & 0xF0F0F0F0u) break;
        v = v * 10000 + parse4SWAR(s + i);
        i += 4;
    }
    // Scalar remaining (up to 18 total)
    while (i < 18 && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    // Find token end
    while (i < 20 && s[i] >= '0' && s[i] <= '9') i++;
    if (i >= 20) {
        while (s + i + 8 <= iEnd) {
            uint64_t d;
            std::memcpy(&d, s + i, 8);
            uint64_t mask = delimiterMask8(d);
            if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done; }
            i += 8;
        }
        while (s[i] >= '0' && s[i] <= '9') i++;
    }
done:
    len = i;
    return v;
}

//=================================================================
// PARSE VER 2: 8-byte first read + SWAR parse8
// Read 8 bytes at once; if all digits, parse8 + check more; else ctz
//=================================================================
#elif PARSE_VER == 2

static inline int64_t parsePositive(const char* s, size_t& len) {
    // Read 8 bytes (safe: mmap + page alignment, but check bounds)
    if (s + 8 > iEnd) {
        // Near end of file: scalar fallback
        int64_t v = 0;
        size_t i = 0;
        while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (s[i] - '0');
            i++;
        }
        len = i;
        return v;
    }
    uint64_t v8;
    std::memcpy(&v8, s, 8);

    if (!allDigits8(v8)) {
        // 1-7 digits: scalar find first non-digit
        int pos = 0;
        while (pos < 8 && s[pos] >= '0' && s[pos] <= '9') pos++;
        len = pos;
        int64_t result = 0;
        for (int i = 0; i < pos; i++)
            result = result * 10 + (s[i] - '0');
        return result;
    }

    // All 8 bytes are digits: parse8
    uint64_t r8 = parse8SWAR(s);

    // Check next 8 bytes for digits 9-16
    if (s + 16 > iEnd) {
        // Near end: scalar tail
        size_t i = 8;
        while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
            r8 = r8 * 10 + (s[i] - '0');
            i++;
        }
        len = i;
        return (int64_t)r8;
    }

    uint64_t v8b;
    std::memcpy(&v8b, s + 8, 8);

    if (!allDigits8(v8b)) {
        // 8 + (1-7) digits, total 9-15
        int pos2 = 0;
        while (pos2 < 8 && s[8 + pos2] >= '0' && s[8 + pos2] <= '9') pos2++;
        for (int i = 0; i < pos2; i++)
            r8 = r8 * 10 + (s[8 + i] - '0');
        len = 8 + pos2;
        return (int64_t)r8;
    }

    // 16 digits: parse second 8
    uint64_t r16 = parse8SWAR(s + 8);
    uint64_t combined = r8 * 100000000ULL + r16;

    // Check for digits 17-18
    if (s[16] >= '0' && s[16] <= '9') {
        combined = combined * 10 + (s[16] - '0');
        if (s[17] >= '0' && s[17] <= '9') {
            combined = combined * 10 + (s[17] - '0');
            len = 18;
            // Find actual end (token could be > 18 digits)
            size_t i = 18;
            while (s + i + 8 <= iEnd) {
                uint64_t d;
                std::memcpy(&d, s + i, 8);
                uint64_t mask = delimiterMask8(d);
                if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done2; }
                i += 8;
            }
            while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') i++;
        done2:
            len = i;
        } else {
            len = 17;
        }
    } else {
        len = 16;
    }
    return (int64_t)combined;
}

//=================================================================
// PARSE VER 3: builtin SIMD (pmaddubsw + pmaddwd)
// Uses __builtin_ia32_* functions for SSSE3/AVX2 instructions
//=================================================================
#elif PARSE_VER == 3

typedef int64_t  v2di __attribute__((vector_size(16)));
typedef int32_t  v4si __attribute__((vector_size(16)));
typedef int16_t  v8hi __attribute__((vector_size(16)));
typedef char     v16qi __attribute__((vector_size(16)));

// Parse exactly 8 ASCII digits using SSSE3 pmaddubsw + SSE2 pmaddwd
// __builtin_ia32_pmaddubsw128: SSSE3, packs 8 bytes -> 4 16-bit
// __builtin_ia32_pmaddwd128: SSE2, packs 4 16-bit -> 2 32-bit
static inline uint32_t parse8digits_builtin(const char* s) {
    // Load 8 bytes into low 64 bits of 128-bit register (high 64 = 0)
    v16qi v = {};
    std::memcpy(&v, s, 8);
    // Subtract '0' (0x30) from each byte to get digit values 0-9
    v16qi zero = __builtin_ia32_psubb128(v, (v16qi){'0','0','0','0','0','0','0','0',0,0,0,0,0,0,0,0});
    // pmaddubsw: multiply byte pairs and add to 16-bit
    //   [d0,d1,d2,d3,d4,d5,d6,d7] * [10,1,10,1,10,1,10,1]
    //   -> [d0*10+d1, d2*10+d3, d4*10+d5, d6*10+d7] as 16-bit
    v16qi mul1 = (v16qi){10,1,10,1,10,1,10,1, 0,0,0,0,0,0,0,0};
    v8hi p1 = __builtin_ia32_pmaddubsw128(zero, mul1);
    // p1 = [d0*10+d1, d2*10+d3, d4*10+d5, d6*10+d7, 0, 0, 0, 0]
    // pmaddwd: multiply 16-bit pairs and add to 32-bit
    //   [pair0,pair1,pair2,pair3] * [100,1,100,1]
    //   -> [pair0*100+pair1, pair2*100+pair3] as 32-bit
    v8hi mul2 = (v8hi){100,1,100,1, 0,0,0,0};
    v4si p2 = __builtin_ia32_pmaddwd128(p1, mul2);
    // p2 = [d0*1000+d1*100+d2*10+d3, d4*1000+d5*100+d6*10+d7, 0, 0]
    // Combine: result = p2[0] * 10000 + p2[1]
    uint32_t lo = ((uint32_t*)&p2)[0];
    uint32_t hi = ((uint32_t*)&p2)[1];
    return lo * 10000u + hi;
}

static inline int64_t parsePositive(const char* s, size_t& len) {
    if (s + 8 > iEnd) {
        int64_t v = 0;
        size_t i = 0;
        while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (s[i] - '0');
            i++;
        }
        len = i;
        return v;
    }
    uint64_t v8;
    std::memcpy(&v8, s, 8);

    if (!allDigits8(v8)) {
        int pos = 0;
        while (pos < 8 && s[pos] >= '0' && s[pos] <= '9') pos++;
        len = pos;
        int64_t result = 0;
        for (int i = 0; i < pos; i++)
            result = result * 10 + (s[i] - '0');
        return result;
    }

    // 8 digits: use builtin SIMD parse
    uint32_t r8 = parse8digits_builtin(s);

    // Check next bytes
    if (s + 16 > iEnd) {
        size_t i = 8;
        uint64_t r = r8;
        while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
            r = r * 10 + (s[i] - '0');
            i++;
        }
        len = i;
        return (int64_t)r;
    }

    uint64_t v8b;
    std::memcpy(&v8b, s + 8, 8);

    if (!allDigits8(v8b)) {
        int pos2 = 0;
        while (pos2 < 8 && s[8 + pos2] >= '0' && s[8 + pos2] <= '9') pos2++;
        uint64_t r = r8;
        for (int i = 0; i < pos2; i++)
            r = r * 10 + (s[8 + i] - '0');
        len = 8 + pos2;
        return (int64_t)r;
    }

    // 16 digits
    uint32_t r16 = parse8digits_builtin(s + 8);
    uint64_t combined = (uint64_t)r8 * 100000000ULL + r16;

    if (s[16] >= '0' && s[16] <= '9') {
        combined = combined * 10 + (s[16] - '0');
        if (s[17] >= '0' && s[17] <= '9') {
            combined = combined * 10 + (s[17] - '0');
            len = 18;
            size_t i = 18;
            while (s + i + 8 <= iEnd) {
                uint64_t d;
                std::memcpy(&d, s + i, 8);
                uint64_t mask = delimiterMask8(d);
                if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done3; }
                i += 8;
            }
            while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') i++;
        done3:
            len = i;
        } else {
            len = 17;
        }
    } else {
        len = 16;
    }
    return (int64_t)combined;
}

//=================================================================
// PARSE VER 4: Merged check+parse (single read per 4-byte group)
// Eliminates redundant memcpy in parse4SWAR by reusing already-loaded
// data. Also flattens the 4-group loop into straight-line code with
// conditional moves to reduce branch mispredictions.
//=================================================================
#elif PARSE_VER == 4

static inline int64_t parsePositive(const char* s, size_t& len) {
    int64_t v = 0;
    size_t i = 0;
    // 4-byte grouped: merged check + parse (single memcpy per group)
    while (i + 4 <= 16) {
        uint32_t d;
        std::memcpy(&d, s + i, 4);
        uint32_t high = d & 0xF0F0F0F0u;
        if (high != 0x30303030u) break;
        uint32_t low = d & 0x0F0F0F0Fu;
        if ((low + 0x06060606u) & 0xF0F0F0F0u) break;
        // Inline parse4SWAR using already-computed low (no second memcpy)
        uint32_t p = low * 2561u;
        p = ((p >> 8) & 0x00FF00FFu) * 6553601u;
        v = v * 10000 + ((p >> 16) & 0xFFFFu);
        i += 4;
    }
    // Scalar remaining (up to 18 total)
    while (i < 18 && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    // Find token end
    while (i < 20 && s[i] >= '0' && s[i] <= '9') i++;
    if (i >= 20) {
        while (s + i + 8 <= iEnd) {
            uint64_t d;
            std::memcpy(&d, s + i, 8);
            uint64_t mask = delimiterMask8(d);
            if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done; }
            i += 8;
        }
        while (s[i] >= '0' && s[i] <= '9') i++;
    }
done:
    len = i;
    return v;
}

//=================================================================
// PARSE VER 5: Branchless 4-byte group parse with conditional masking
// Always parses all 4 groups, masks invalid results to avoid branch
// misprediction. Uses cmov-friendly ternary for v update.
//=================================================================
#elif PARSE_VER == 5

static inline int64_t parsePositive(const char* s, size_t& len) {
    int64_t v = 0;
    size_t i = 0;
    // Branchless 4-byte grouped: always parse, mask invalid
    // Sticky valid: once a group is invalid, all subsequent groups are too
    // (prevents reading next token's digits as part of current token)
    uint32_t active = 1;
    for (int g = 0; g < 4; g++) {
        uint32_t d;
        std::memcpy(&d, s + g * 4, 4);
        uint32_t high = d & 0xF0F0F0F0u;
        uint32_t low = d & 0x0F0F0F0Fu;
        // valid: 1 if all 4 bytes are digits AND previous groups were valid
        uint32_t high_ok = (high == 0x30303030u);
        uint32_t low_ok = !((low + 0x06060606u) & 0xF0F0F0F0u);
        uint32_t valid = (high_ok & low_ok) & active;
        active = valid;  // sticky: once 0, stays 0
        // Parse 4 digits from low nibbles
        uint32_t p = low * 2561u;
        p = ((p >> 8) & 0x00FF00FFu) * 6553601u;
        p = (p >> 16) & 0xFFFFu;
        // Conditional update (compiler should emit cmov/branchless)
        int64_t new_v = v * 10000 + (int64_t)p;
        v = valid ? new_v : v;
        i += 4 * (size_t)valid;
    }
    // Scalar remaining (up to 18 total)
    while (i < 18 && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    // Find token end
    while (i < 20 && s[i] >= '0' && s[i] <= '9') i++;
    if (i >= 20) {
        while (s + i + 8 <= iEnd) {
            uint64_t d;
            std::memcpy(&d, s + i, 8);
            uint64_t mask = delimiterMask8(d);
            if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done; }
            i += 8;
        }
        while (s[i] >= '0' && s[i] <= '9') i++;
    }
done:
    len = i;
    return v;
}

//=================================================================
// PARSE VER 6: AVX2 builtin 16-byte parse (pmaddubsw256 + pmaddwd256)
// Fast path: 16 digits parsed via 3 SIMD instructions (psubb + pmaddubsw + pmaddwd)
// Fallback: V4 merged for < 16 digits
// Digit check: SWAR allDigits8 x2 (reuses existing nibble-based check)
//=================================================================
#elif PARSE_VER == 6

typedef short v16hi_avx __attribute__((vector_size(32)));
typedef int v8si_avx __attribute__((vector_size(32)));
typedef char v32qi_avx __attribute__((vector_size(32)));

// Static const AVX2 vectors (loaded to ymm registers by compiler)
static const v32qi_avx AVX_ZERO = (v32qi_avx){
    '0','0','0','0','0','0','0','0',
    '0','0','0','0','0','0','0','0',
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
};
static const v32qi_avx AVX_MUL1 = (v32qi_avx){
    10,1,10,1,10,1,10,1, 10,1,10,1,10,1,10,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
};
static const v16hi_avx AVX_MUL2 = (v16hi_avx){
    100,1,100,1,100,1,100,1, 100,1,100,1,100,1,100,1
};

static inline int64_t parsePositive(const char* s, size_t& len) {
    // Fast path: check 16 bytes all digits, then AVX2 parse
    if (s + 18 <= iEnd) {
        uint64_t lo, hi;
        std::memcpy(&lo, s, 8);
        std::memcpy(&hi, s + 8, 8);
        if (allDigits8(lo) && allDigits8(hi)) {
            // 16 digits confirmed: AVX2 parse via psubb + pmaddubsw + pmaddwd
            v32qi_avx v;
            std::memcpy(&v, s, 16);
            v = __builtin_ia32_psubb256(v, AVX_ZERO);
            v16hi_avx p1 = __builtin_ia32_pmaddubsw256(v, AVX_MUL1);
            v8si_avx p2 = __builtin_ia32_pmaddwd256(p1, AVX_MUL2);
            // p2 = [d0d1d2d3, d4d5d6d7, d8d9d10d11, d12d13d14d15, 0,0,0,0]
            const uint32_t* pp = (const uint32_t*)&p2;
            // Combine 4 limbs (each 0-9999) into int64
            // 10^16 < INT64_MAX (9.22*10^18), safe
            int64_t result = (int64_t)pp[0] * 1000000000000LL
                           + (int64_t)pp[1] * 100000000LL
                           + (int64_t)pp[2] * 10000LL
                           + (int64_t)pp[3];
            // Check digits 17-18 (scalar, most tokens are <= 18 digits)
            size_t i = 16;
            if (s[16] >= '0' && s[16] <= '9') {
                result = result * 10 + (s[16] - '0');
                i = 17;
                if (s[17] >= '0' && s[17] <= '9') {
                    result = result * 10 + (s[17] - '0');
                    i = 18;
                    // Token > 18 digits: SWAR scan for delimiter
                    while (s + i + 8 <= iEnd) {
                        uint64_t d;
                        std::memcpy(&d, s + i, 8);
                        uint64_t mask = delimiterMask8(d);
                        if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done; }
                        i += 8;
                    }
                    while (s[i] >= '0' && s[i] <= '9') i++;
                }
            }
        done:
            len = i;
            return result;
        }
    }
    // Fallback: V4 merged check+parse for < 16 digits (or near EOF)
    int64_t v = 0;
    size_t i = 0;
    while (i + 4 <= 16) {
        uint32_t d;
        std::memcpy(&d, s + i, 4);
        uint32_t high = d & 0xF0F0F0F0u;
        if (high != 0x30303030u) break;
        uint32_t low = d & 0x0F0F0F0Fu;
        if ((low + 0x06060606u) & 0xF0F0F0F0u) break;
        uint32_t p = low * 2561u;
        p = ((p >> 8) & 0x00FF00FFu) * 6553601u;
        v = v * 10000 + ((p >> 16) & 0xFFFFu);
        i += 4;
    }
    while (i < 18 && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        i++;
    }
    while (i < 20 && s[i] >= '0' && s[i] <= '9') i++;
    if (i >= 20) {
        while (s + i + 8 <= iEnd) {
            uint64_t d;
            std::memcpy(&d, s + i, 8);
            uint64_t mask = delimiterMask8(d);
            if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done2; }
            i += 8;
        }
        while (s[i] >= '0' && s[i] <= '9') i++;
    }
done2:
    len = i;
    return v;
}

//=================================================================
// PARSE VER 7: 8-byte umask check + ctz length + shift-align SWAR
// Based on rogeryoungh's swar_64 technique:
//   umask = u & (u + 0x06..06) & 0xF0..F0
//   If umask == 0x3030..30: all 8 bytes are digits
//   Else: dl = ctz(umask ^ 0x3030..30) >> 3 = digit count
//   Shift align: u <<= 64 - (dl << 3), then SWAR parse 8 bytes
//   (low bytes become 0x00, nibble=0, treated as digit value 0, no effect)
// Advantage: one 8-byte check replaces 2x 4-byte loops for 5-8 digit tokens
//=================================================================
#elif PARSE_VER == 7

// SWAR parse 8 ASCII digits from uint64 value (no memcpy needed)
static inline uint64_t parse8SWAR_u64(uint64_t u) {
    u = (u & 0x0F0F0F0F0F0F0F0FULL) * 2561ULL;
    u = ((u >> 8) & 0x00FF00FF00FF00FFULL) * 6553601ULL;
    u = ((u >> 16) & 0x0000FFFF0000FFFFULL) * 42949672960001ULL;
    return u >> 32;
}

static inline int64_t parsePositive(const char* s, size_t& len) {
    if (s + 8 > iEnd) {
        // Near EOF: scalar fallback
        int64_t v = 0;
        size_t i = 0;
        while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (s[i] - '0');
            i++;
        }
        len = i;
        return v;
    }

    uint64_t u;
    std::memcpy(&u, s, 8);

    constexpr uint64_t cx30 = 0x3030303030303030ULL;
    uint64_t umask = u & (u + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL;

    if (umask == cx30) {
        // All 8 bytes are digits: parse8 + check more
        uint64_t r = parse8SWAR_u64(u);
        size_t i = 8;

        // Check next 8 bytes for digits 9-16
        if (s + 16 > iEnd) {
            // Near EOF after 8 digits
            while (s + i < iEnd && s[i] >= '0' && s[i] <= '9') {
                r = r * 10 + (s[i] - '0');
                i++;
            }
            len = i;
            return (int64_t)r;
        }

        uint64_t u2;
        std::memcpy(&u2, s + 8, 8);
        uint64_t umask2 = u2 & (u2 + 0x0606060606060606ULL) & 0xF0F0F0F0F0F0F0F0ULL;

        if (umask2 == cx30) {
            // 16 digits: parse second 8 and combine
            uint64_t r2 = parse8SWAR_u64(u2);
            r = r * 100000000ULL + r2;
            i = 16;
            // Check digits 17-18 (scalar, most tokens <= 18 digits)
            if (s[16] >= '0' && s[16] <= '9') {
                r = r * 10 + (s[16] - '0');
                i = 17;
                if (s[17] >= '0' && s[17] <= '9') {
                    r = r * 10 + (s[17] - '0');
                    i = 18;
                    // Token > 18 digits: SWAR scan for delimiter
                    while (s + i + 8 <= iEnd) {
                        uint64_t d;
                        std::memcpy(&d, s + i, 8);
                        uint64_t mask = delimiterMask8(d);
                        if (mask) { i += size_t(__builtin_ctzll(mask)) / 8; goto done7; }
                        i += 8;
                    }
                    while (s[i] >= '0' && s[i] <= '9') i++;
                }
            done7:
                len = i;
                return (int64_t)r;
            }
            len = i;
            return (int64_t)r;
        } else {
            // 8 + (0-7) digits: shift-align second chunk
            uint64_t dl2 = __builtin_ctzll(umask2 ^ cx30) >> 3;
            if (dl2 > 0) {
                u2 <<= 64 - (dl2 << 3);
                uint64_t r2 = parse8SWAR_u64(u2);
                static const uint64_t p10[] = {1, 10, 100, 1000, 10000,
                                               100000, 1000000, 10000000};
                r = r * p10[dl2] + r2;
                i = 8 + dl2;
            }
            len = i;
            return (int64_t)r;
        }
    }

    // Not all 8 bytes are digits: compute length via ctz
    uint64_t dl = __builtin_ctzll(umask ^ cx30) >> 3;
    if (dl == 0) {
        // First byte not a digit (shouldn't happen in valid input)
        len = 0;
        return 0;
    }

    // Shift align + SWAR parse (rogeryoungh technique)
    // After shift, low (8-dl) bytes become 0x00, nibble=0, treated as digit 0
    u <<= 64 - (dl << 3);
    uint64_t r = parse8SWAR_u64(u);

    len = dl;
    return (int64_t)r;
}

#endif // PARSE_VER

//=================================================================
// WRITE VER 1: 10000-base decomposition + outTable (baseline)
//=================================================================
#if WRITE_VER == 1

static inline void writeI64(int64_t val) {
    if (val == 0) { *oCursor++ = '0'; return; }
    uint64_t uv;
    bool neg = val < 0;
    if (neg) {
        *oCursor++ = '-';
        uv = uint64_t(-(val + 1)) + 1;
    } else {
        uv = uint64_t(val);
    }
    uint32_t limbs[5];
    int n = 0;
    while (uv >= 10000) {
        limbs[n++] = uint32_t(uv % 10000);
        uv /= 10000;
    }
    limbs[n++] = uint32_t(uv);
    uint32_t high = limbs[n - 1];
    if (high < 10) {
        *oCursor++ = char('0' + high);
    } else if (high < 100) {
        *oCursor++ = char('0' + high / 10);
        *oCursor++ = char('0' + high % 10);
    } else if (high < 1000) {
        *oCursor++ = char('0' + high / 100);
        *oCursor++ = char('0' + high / 10 % 10);
        *oCursor++ = char('0' + high % 10);
    } else {
        std::memcpy(oCursor, &outTable[high], 4);
        oCursor += 4;
    }
    for (int j = n - 2; j >= 0; j--) {
        std::memcpy(oCursor, &outTable[limbs[j]], 4);
        oCursor += 4;
    }
}

//=================================================================
// WRITE VER 2: Barrett reduction (avoid div) + SSE2 4-byte store
//=================================================================
#elif WRITE_VER == 2

// Barrett reduction: replace uv % 10000 and uv /= 10000 with multiply
// div10000(uv) = uv / 10000, mod10000(uv) = uv % 10000
// Using magic number: floor(2^64 / 10000) = 1844674407370955 (0x68DB8BAC710CB)
static constexpr uint64_t BARRETT_M = 0x68DB8BAC710CBULL; // floor(2^64 / 10000)
// q = (uv * BARRETT_M) >> 64  approximates uv / 10000
// r = uv - q * 10000

static inline void writeI64(int64_t val) {
    if (val == 0) { *oCursor++ = '0'; return; }
    uint64_t uv;
    bool neg = val < 0;
    if (neg) {
        *oCursor++ = '-';
        uv = uint64_t(-(val + 1)) + 1;
    } else {
        uv = uint64_t(val);
    }
    uint32_t limbs[5];
    int n = 0;
    while (uv >= 10000) {
        // Barrett reduction
        uint64_t q = (unsigned __int128)uv * BARRETT_M >> 64;
        uint32_t r = uint32_t(uv - q * 10000);
        // Correct: r may be slightly off due to truncation
        if (r >= 10000) { r -= 10000; q++; }
        limbs[n++] = r;
        uv = q;
    }
    limbs[n++] = uint32_t(uv);
    uint32_t high = limbs[n - 1];
    if (high < 10) {
        *oCursor++ = char('0' + high);
    } else if (high < 100) {
        *oCursor++ = char('0' + high / 10);
        *oCursor++ = char('0' + high % 10);
    } else if (high < 1000) {
        *oCursor++ = char('0' + high / 100);
        *oCursor++ = char('0' + high / 10 % 10);
        *oCursor++ = char('0' + high % 10);
    } else {
        std::memcpy(oCursor, &outTable[high], 4);
        oCursor += 4;
    }
    for (int j = n - 2; j >= 0; j--) {
        std::memcpy(oCursor, &outTable[limbs[j]], 4);
        oCursor += 4;
    }
}

#endif // WRITE_VER

// === Main: ADD benchmark (same I/O format as LC ADD) ===
int main() {
    initOutTable();
    initInput();

    // Read T (number of test cases)
    size_t t = 0;
    while (iCursor < iEnd && *iCursor >= '0' && *iCursor <= '9') {
        t = t * 10 + size_t(*iCursor++ - '0');
    }
    if (iCursor < iEnd && *iCursor < 0x21) iCursor++;

    // Optimized main loop: branchless delimiter skip using arithmetic
    // Safe because mmap pages are zero-padded beyond iEnd
    while (t--) {
        size_t la;
        int64_t va = parsePositive(iCursor, la);
        iCursor += la + (size_t)(iCursor[la] < 0x21);

        size_t lb;
        int64_t vb = parsePositive(iCursor, lb);
        iCursor += lb + (size_t)(iCursor[lb] < 0x21);

        writeI64(va + vb);
        *oCursor++ = '\n';
    }

    flushOutput();
    return 0;
}
