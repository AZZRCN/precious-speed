// gmpref.cpp — GMP 理论标尺 (仅本地参照, 不可提交 LC)
// 与 HEX multiplication 同 I/O 契约; 分段计时打到 stderr。
// build: g++ -O2 -std=c++23 -march=znver3 -mtune=znver3 gmpref.cpp -o gmpref -lgmp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#include <gmp.h>
#include <unistd.h>

using clk = std::chrono::steady_clock;
static double ms(clk::time_point a, clk::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

static std::vector<char> slurp() {
    std::vector<char> buf;
    buf.reserve(1 << 22);
    char tmp[1 << 20];
    ssize_t n;
    while ((n = read(0, tmp, sizeof tmp)) > 0) buf.insert(buf.end(), tmp, tmp + n);
    buf.push_back('\0');
    return buf;
}

int main() {
    double t_parse = 0, t_mul = 0, t_fmt = 0;
    auto T0 = clk::now();
    auto in = slurp();
    auto T1 = clk::now();

    char *p = in.data();
    long T = strtol(p, &p, 10);

    std::vector<char> out;
    out.reserve(1 << 23);

    mpz_t a, b, c;
    mpz_inits(a, b, c, NULL);
    // 复用一块 format 缓冲, 避免 malloc 抖动
    std::vector<char> fbuf;

    for (long i = 0; i < T; i++) {
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        char *s1 = p;
        while (*p && *p != ' ') p++;
        *p++ = '\0';
        while (*p == ' ') p++;
        char *s2 = p;
        while (*p && *p != ' ' && *p != '\n' && *p != '\r') p++;
        char sav = *p;
        *p = '\0';

        auto q0 = clk::now();
        mpz_set_str(a, s1, 16);
        mpz_set_str(b, s2, 16);
        auto q1 = clk::now();
        mpz_mul(c, a, b);
        auto q2 = clk::now();
        size_t need = mpz_sizeinbase(c, 16) + 3;
        if (fbuf.size() < need) fbuf.resize(need);
        mpz_get_str(fbuf.data(), -16, c);      // 负基数 => 大写
        size_t L = strlen(fbuf.data());
        out.insert(out.end(), fbuf.data(), fbuf.data() + L);
        out.push_back('\n');
        auto q3 = clk::now();

        t_parse += ms(q0, q1);
        t_mul   += ms(q1, q2);
        t_fmt   += ms(q2, q3);
        *p = sav;
    }
    auto T2 = clk::now();
    if (!out.empty()) {
        size_t off = 0;
        while (off < out.size()) {
            ssize_t w = write(1, out.data() + off, out.size() - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
    }
    auto T3 = clk::now();
    fprintf(stderr, "GMPREF read=%.2f parse=%.2f mul=%.2f fmt=%.2f write=%.2f total=%.2f\n",
            ms(T0, T1), t_parse, t_mul, t_fmt, ms(T2, T3), ms(T0, T3));
    return 0;
}
