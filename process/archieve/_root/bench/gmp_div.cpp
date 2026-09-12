// GMP 除法基准测试: 与 CUR div.cpp 相同 I/O 格式
// 编译: g++ -O2 -std=gnu++20 -lgmp gmp_div.cpp -o gmp_div
#include <gmp.h>
#include <cstdio>
#include <cstring>
#include <chrono>

// 快速输入
static char *iBuf = nullptr;
static size_t iBufSize = 0, iPos = 0, iEnd = 0;

static inline void initInput() {
    iBufSize = 1 << 24;  // 16MB initial
    iBuf = (char*)malloc(iBufSize);
    size_t total = 0;
    while (true) {
        if (total + (1<<20) > iBufSize) {
            iBufSize <<= 1;
            iBuf = (char*)realloc(iBuf, iBufSize);
        }
        size_t n = fread(iBuf + total, 1, (1<<20), stdin);
        total += n;
        if (n == 0) break;
    }
    iEnd = total;
}

static inline char *readToken() {
    while (iPos < iEnd && (unsigned char)iBuf[iPos] <= 0x20) iPos++;
    if (iPos >= iEnd) return nullptr;
    char *start = iBuf + iPos;
    while (iPos < iEnd && (unsigned char)iBuf[iPos] > 0x20) iPos++;
    if (iPos < iEnd) iBuf[iPos] = 0;
    return start;
}

// 快速输出
static char *oBuf = nullptr;
static size_t oPos = 0, oBufSize = 0;

static inline void flushOutput() {
    fwrite(oBuf, 1, oPos, stdout);
    oPos = 0;
}

static inline void ensureOut(size_t need) {
    if (oPos + need > oBufSize) {
        oBufSize = (oBufSize ? oBufSize : (1<<20)) * 2;
        oBuf = (char*)realloc(oBuf, oBufSize);
    }
}

int main() {
    initInput();
    oBufSize = 1 << 24;
    oBuf = (char*)malloc(oBufSize);

    char *t_str = readToken();
    size_t t = 0;
    for (size_t i = 0; t_str[i]; i++) t = t * 10 + (t_str[i] - '0');

    mpz_t a, b, q, r;
    mpz_init(a); mpz_init(b); mpz_init(q); mpz_init(r);

    // 预读所有输入
    for (size_t i = 0; i < t; i++) {
        char *sa = readToken();
        char *sb = readToken();
        if (!sa || !sb) break;

        mpz_set_str(a, sa, 10);
        mpz_set_str(b, sb, 10);

        mpz_tdiv_qr(q, r, a, b);

        // 输出 q r
        ensureOut(mpz_sizeinbase(q, 10) + mpz_sizeinbase(r, 10) + 4);
        if (mpz_sgn(q) == 0) {
            oBuf[oPos++] = '0';
        } else {
            mpz_get_str(oBuf + oPos, 10, q);
            oPos += strlen(oBuf + oPos);
        }
        oBuf[oPos++] = ' ';
        if (mpz_sgn(r) == 0) {
            oBuf[oPos++] = '0';
        } else {
            mpz_get_str(oBuf + oPos, 10, r);
            oPos += strlen(oBuf + oPos);
        }
        oBuf[oPos++] = '\n';
    }

    flushOutput();
    mpz_clear(a); mpz_clear(b); mpz_clear(q); mpz_clear(r);
    free(iBuf); free(oBuf);
    return 0;
}
