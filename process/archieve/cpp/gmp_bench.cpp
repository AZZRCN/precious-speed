// GMP 基准测试 - 用于对比 moptm 的理论上限
// 编译: g++ -O2 -I C:\msys64\mingw64\include -L C:\msys64\mingw64\lib -o gmp_add.exe gmp_bench.cpp -lgmp -DGMP_OP_ADD
//       g++ -O2 -I C:\msys64\mingw64\include -L C:\msys64\mingw64\lib -o gmp_mul.exe gmp_bench.cpp -lgmp -DGMP_OP_MUL
//       g++ -O2 -I C:\msys64\mingw64\include -L C:\msys64\mingw64\lib -o gmp_div.exe gmp_bench.cpp -lgmp -DGMP_OP_DIV
#include <gmp.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int t = 1;
    std::cin >> t;

#if defined(GMP_OP_ADD)
    // 输出缓冲
    std::string out;
    out.reserve(32 << 20);
    mpz_t a, b, c;
    mpz_init(a); mpz_init(b); mpz_init(c);
    auto t0 = std::chrono::high_resolution_clock::now();
    while (t--) {
        std::string sa, sb;
        std::cin >> sa >> sb;
        mpz_set_str(a, sa.c_str(), 10);
        mpz_set_str(b, sb.c_str(), 10);
        mpz_add(c, a, b);
        char *s = mpz_get_str(nullptr, 10, c);
        out += s;
        out += '\n';
        free(s);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    fwrite(out.data(), 1, out.size(), stdout);
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    fprintf(stderr, "GMP ADD: %.3f ms\n", ms);
    mpz_clear(a); mpz_clear(b); mpz_clear(c);

#elif defined(GMP_OP_MUL)
    std::string out;
    out.reserve(32 << 20);
    mpz_t a, b, c;
    mpz_init(a); mpz_init(b); mpz_init(c);
    auto t0 = std::chrono::high_resolution_clock::now();
    while (t--) {
        std::string sa, sb;
        std::cin >> sa >> sb;
        mpz_set_str(a, sa.c_str(), 10);
        mpz_set_str(b, sb.c_str(), 10);
        mpz_mul(c, a, b);
        char *s = mpz_get_str(nullptr, 10, c);
        out += s;
        out += '\n';
        free(s);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    fwrite(out.data(), 1, out.size(), stdout);
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    fprintf(stderr, "GMP MUL: %.3f ms\n", ms);
    mpz_clear(a); mpz_clear(b); mpz_clear(c);

#elif defined(GMP_OP_DIV)
    std::string out;
    out.reserve(32 << 20);
    mpz_t a, b, q, r;
    mpz_init(a); mpz_init(b); mpz_init(q); mpz_init(r);
    auto t0 = std::chrono::high_resolution_clock::now();
    while (t--) {
        std::string sa, sb;
        std::cin >> sa >> sb;
        mpz_set_str(a, sa.c_str(), 10);
        mpz_set_str(b, sb.c_str(), 10);
        mpz_tdiv_qr(q, r, a, b);
        char *sq = mpz_get_str(nullptr, 10, q);
        char *sr = mpz_get_str(nullptr, 10, r);
        out += sq; out += ' '; out += sr; out += '\n';
        free(sq); free(sr);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    fwrite(out.data(), 1, out.size(), stdout);
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    fprintf(stderr, "GMP DIV: %.3f ms\n", ms);
    mpz_clear(a); mpz_clear(b); mpz_clear(q); mpz_clear(r);

#else
#error "Must define GMP_OP_ADD, GMP_OP_MUL, or GMP_OP_DIV"
#endif
    return 0;
}
