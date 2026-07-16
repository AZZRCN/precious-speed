// bench_all.cpp — 四方对比：ORIG / OPT / GMP / Boost
// 编译:
//   g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o bench_orig.exe bench_all.cpp
//   g++ -O3 -mavx2 -mfma -funroll-loops -o bench_opt.exe bench_all.cpp
//   g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_GMP -o bench_gmp.exe bench_all.cpp C:\msys64\mingw64\lib\libgmp.a
//   g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_BOOST -IE:\boost -o bench_boost2.exe bench_all.cpp
// 运行: bench_xxx.exe <round> <scale> <op> <tag>
// op: mul | sqr | div2
// 输出: tag,time_ms
#include <iostream>
#include <chrono>
#include <string>
#include <cstdlib>

#ifdef USE_GMP
#include "gmp.h"
#elif defined(USE_BOOST)
#include <boost/multiprecision/cpp_int.hpp>
namespace bmp = boost::multiprecision;
#else
#ifdef USE_ORIGINAL
#include "../masonxiong.cpp"
#else
#include "masonxiong_opt.cpp"
#endif
#endif

std::string genRandomDigits(size_t n, unsigned seed) {
    std::string s(n, '0');
    srand(seed);
    s[0] = '1' + (rand() % 9);
    for (size_t i = 1; i < n; ++i) s[i] = '0' + (rand() % 10);
    return s;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "usage: " << argv[0] << " <round> <scale> <op> <tag>" << std::endl;
        return 1;
    }
    int round = atoi(argv[1]);
    size_t scale = atoi(argv[2]);
    std::string op = argv[3];
    std::string tag = argv[4];

    std::string sa = genRandomDigits(scale, 42);

    auto t0 = std::chrono::high_resolution_clock::now();

#if defined(USE_GMP)
    mpz_t a, a2, result;
    mpz_init(a); mpz_init(a2); mpz_init(result);
    mpz_set_str(a, sa.c_str(), 10);
    mpz_mul(a2, a, a);
    for (int i = 0; i < round; ++i) {
        if (op == "mul") mpz_mul(result, a, a);
        else if (op == "sqr") mpz_mul(result, a, a);
        else if (op == "div2") mpz_tdiv_q(result, a2, a);
    }
    mpz_clear(a); mpz_clear(a2); mpz_clear(result);
#elif defined(USE_BOOST)
    bmp::cpp_int a(sa);
    bmp::cpp_int a2 = a * a;
    bmp::cpp_int result;
    for (int i = 0; i < round; ++i) {
        if (op == "mul") result = a * a;
        else if (op == "sqr") result = a * a;
        else if (op == "div2") result = a2 / a;
    }
#else
    UnsignedInteger a(sa.c_str());
    UnsignedInteger a2 = a * a;
    UnsignedInteger result;
    for (int i = 0; i < round; ++i) {
        if (op == "mul") result = a * a;
        else if (op == "sqr") {
#ifdef USE_ORIGINAL
            result = a * a;
#else
            result = a.square();
#endif
        }
        else if (op == "div2") result = a2 / a;
    }
#endif

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << tag << "," << ms << std::endl;
    return 0;
}
