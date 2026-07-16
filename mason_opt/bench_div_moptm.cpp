// bench_div_moptm.cpp - moptm vs masonxiong_opt 除法性能对比
// 编译:
//   g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_MOPTM -o bench_moptm.exe bench_div_moptm.cpp
//   g++ -O3 -mavx2 -mfma -funroll-loops -o bench_mason.exe bench_div_moptm.cpp
// 运行: bench_xxx.exe <round> <na> <nb> <tag>
#include <iostream>
#include <chrono>
#include <string>
#include <cstdlib>

#ifdef USE_MOPTM
#include "moptm.cpp"
#else
#include "masonxiong_opt.cpp"
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
        std::cerr << "usage: " << argv[0] << " <round> <na> <nb> <tag>" << std::endl;
        return 1;
    }
    int round = atoi(argv[1]);
    size_t na = atoi(argv[2]);
    size_t nb = atoi(argv[3]);
    std::string tag = argv[4];

    std::string sa = genRandomDigits(na, 42);
    std::string sb = genRandomDigits(nb, 43);
    UnsignedInteger a(sa.c_str()), b(sb.c_str());

    auto t0 = std::chrono::high_resolution_clock::now();
    std::pair<UnsignedInteger, UnsignedInteger> result;
    for (int i = 0; i < round; ++i) {
        result = a.divisionAndModulus(b);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << tag << "," << ms << std::endl;
    (void)result;
    return 0;
}
