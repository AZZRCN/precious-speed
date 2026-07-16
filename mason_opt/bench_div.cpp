// bench_div.cpp — div_1st(原始) vs div_work(优化中) 除法对比
// 编译:
//   g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o bench_div_orig.exe bench_div.cpp
//   g++ -O3 -mavx2 -mfma -funroll-loops -o bench_div_work.exe bench_div.cpp
// 运行: bench_div_xxx.exe <round> <scale> <op> <tag>
// op: div2 | div | sqr | mul
// 输出: tag,time_ms
#include <iostream>
#include <chrono>
#include <string>
#include <cstdlib>

// 屏蔽被包含文件末尾的 main()，避免符号冲突
#define main unused_main
#ifdef USE_ORIGINAL
#include "div_1st.cpp"
#else
#include "div_work.cpp"
#endif
#undef main

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
    std::string sb = genRandomDigits(scale, 43);
    // divk: a 有 scale*k 位, b 有 scale 位, blocks=k (走 Core2)
    std::string sak;
    hint::Integer a(sa.c_str()), b(sb.c_str());
    hint::Integer a2 = a * a;
    hint::Integer aBig = a > b ? a : b;
    hint::Integer bSmall = a > b ? b : a;
    hint::Integer result;
    if (op == "div3" || op == "div4" || op == "div5" || op == "div8") {
        int k = atoi(op.c_str() + 3);
        sak = genRandomDigits(scale * k, 44);
        aBig = hint::Integer(sak.c_str());
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < round; ++i) {
        if (op == "mul") result = a * b;
        else if (op == "sqr") result = a * a;
        else if (op == "div2") result = a2 / a;
        else if (op == "div") result = aBig / bSmall;
        else if (op == "div3" || op == "div4" || op == "div5" || op == "div8") result = aBig / b;
        else { std::cerr << "unknown op: " << op << std::endl; return 1; }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << tag << "," << ms << std::endl;
    (void)result;
    return 0;
}
