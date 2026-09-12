// HEX 加法 · 朴素标量 I/O 基线（avx2_opt 演示用）
// 刻意保留两个会被优化器命中的标量热惯用法：
//   T1: while(*p <= ' ') ++p;        —— 标量空白跳过
//   T2: 逐字节写十六进制               —— 标量输出
// 优化器应自动检测并改写为 SIMD 等价实现，且 bytecmp 闸门通过。
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <unistd.h>

static const int CAP = 64 << 20;
static char inbuf[CAP + 128];
static char outbuf[CAP + 128];

static inline unsigned long long hexval(const char* s, int n) {
    unsigned long long v = 0;
    for (int i = 0; i < n; ++i) {
        char c = s[i];
        int d = c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
        v = (v << 4) | (unsigned)(d & 15);
    }
    return v;
}
static inline int hexlen(const char* p) {
    int n = 0;
    while (p[n] > ' ' && p[n] != '\n') ++n;
    return n;
}

int main() {
    int len = 0;
    while (1) {
        long r = read(0, inbuf + len, CAP - len);
        if (r <= 0) break;
        len += (int)r;
    }
    const char* p = inbuf;
    while (*p <= ' ') ++p;                 // T1 (前导空白)
    unsigned long long T = 0;
    while (*p > ' ') { T = T * 10 + (*p++ - '0'); }
    char* out = outbuf;
    while (T--) {
        while (*p <= ' ') ++p;             // T1 (A 前)
        const char* a0 = p;
        int la = hexlen(p);
        p += la;
        while (*p <= ' ') ++p;             // T1 (B 前)
        const char* b0 = p;
        int lb = hexlen(p);
        p += lb;
        unsigned long long a = hexval(a0, la), b = hexval(b0, lb);
        unsigned long long s = a + b;
        // T2: 标量逐字节写十六进制
        if (s == 0) {
            *out++ = '0';
        } else {
            char tmp[20];
            int n = 0;
            static const char* H = "0123456789abcdef";
            while (s) { tmp[n++] = H[s & 15]; s >>= 4; }
            for (int i = n - 1; i >= 0; --i) *out++ = tmp[i];
        }
        *out++ = '\n';
    }
    long off = 0, n = out - outbuf;
    while (off < n) {
        long w = write(1, outbuf + off, n - off);
        if (w <= 0) break;
        off += w;
    }
    return 0;
}
