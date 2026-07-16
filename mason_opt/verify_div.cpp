// verify_div.cpp — 验证 div_work.cpp 除法正确性: q*B + r == A 且 0 <= r < B
// 编译: g++ -O3 -mavx2 -mfma -funroll-loops -o verify_div.exe verify_div.cpp
// 运行: verify_div.exe <scale> <rounds>
#include <iostream>
#include <chrono>
#include <string>
#include <cstdlib>

#define main unused_main
#include "div_work.cpp"
#undef main

using hint::Integer;

std::string genRandomDigits(size_t n, unsigned seed) {
    std::string s(n, '0');
    srand(seed);
    s[0] = '1' + (rand() % 9);
    for (size_t i = 1; i < n; ++i) s[i] = '0' + (rand() % 10);
    return s;
}

int main(int argc, char* argv[]) {
    size_t scale = argc > 1 ? atoi(argv[1]) : 100000;
    int rounds = argc > 2 ? atoi(argv[2]) : 5;
    unsigned seed = 100;

    for (int t = 0; t < rounds; ++t) {
        // 生成不同规模的 A 和 B
        size_t na = scale;
        size_t nb = (t % 4 == 0) ? scale : (t % 4 == 1 ? scale / 2 : (t % 4 == 2 ? scale * 2 : 64));
        if (nb < 2) nb = 2;
        std::string sa = genRandomDigits(na, seed++);
        std::string sb = genRandomDigits(nb, seed++);
        Integer a(sa.c_str()), b(sb.c_str());
        Integer q, r;
        a.absDivRem(b, q, r);
        // 验证: q*B + r == A 且 0 <= r < B
        Integer check = q * b + r;
        if (check != a) {
            std::cerr << "FAIL round " << t << ": q*B+r != A (na=" << na << " nb=" << nb << ")\n";
            return 1;
        }
        if (r >= b && !r.isZero() == false && r.isZero() == false) {
            // r 必须满足 0 <= r < B
        }
        if (!(r < b)) {
            std::cerr << "FAIL round " << t << ": r >= B (na=" << na << " nb=" << nb << ")\n";
            return 1;
        }
        // 确保 r >= 0 (UnsignedInteger 始终非负，检查符号)
        if (r.isNeg()) {
            std::cerr << "FAIL round " << t << ": r < 0\n";
            return 1;
        }
        std::cout << "OK round " << t << " na=" << na << " nb=" << nb
                  << " qlen=" << static_cast<std::string>(q).size()
                  << " rlen=" << static_cast<std::string>(r).size() << "\n";
    }
    std::cout << "ALL PASS\n";
    return 0;
}
