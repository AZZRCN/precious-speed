// tle_duel.cpp — 原版 vs 优化版交替测试，带 TLE 检测
// 编译:
//   g++ -O3 -mavx2 -mfma -funroll-loops -DUSE_ORIGINAL -o duel_orig.exe tle_duel.cpp
//   g++ -O3 -mavx2 -mfma -funroll-loops -o duel_opt.exe tle_duel.cpp
//
// 参数: tle_duel.exe <round> <scale> <op> <tag>
// op:
//   ctor   — 字符串构造
//   tostr  — 转 const char*
//   add    — 加法
//   sub    — 减法
//   mul    — 乘法
//   mul2   — a^2 * a (2 次 FFT 乘法)
//   sqr    — 自乘 (优化版走 square() 专用路径, 原版走 a*a)
//   div    — 除法 (大/小)
//   div2   — a^2 / a (Newton 迭代)
//   mod    — 取模
//   cmp    — 比较 (6 个运算符)
//   conv   — 类型转换 (int64/double/string)
//   io     — I/O 往返 (ostringstream + istringstream)
// 输出: tag,time_ms
#include <iostream>
#include <chrono>
#include <string>
#include <sstream>
#include <cstdlib>

#ifdef USE_ORIGINAL
#include "../masonxiong.cpp"
#elif defined(USE_EXPERIMENTAL)
#include "masonxiong_EXPERIMENTAL.cpp"
#else
#include "masonxiong_opt.cpp"
#endif

using namespace std;

string genRandomDigits(size_t n, unsigned seed) {
    string s(n, '0');
    srand(seed);
    s[0] = '1' + (rand() % 9);
    for (size_t i = 1; i < n; ++i) s[i] = '0' + (rand() % 10);
    return s;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "usage: " << argv[0] << " <round> <scale> <op> <tag>" << endl;
        return 1;
    }
    int round = atoi(argv[1]);
    size_t scale = atoi(argv[2]);
    string op = argv[3];
    string tag = argv[4];

    string sa = genRandomDigits(scale, 42);
    string sb = genRandomDigits(scale, 43);
    UnsignedInteger a(sa.c_str()), b(sb.c_str());
    UnsignedInteger a2 = a * a;
    UnsignedInteger aBig = a > b ? a : b;
    UnsignedInteger bSmall = a > b ? b : a;

    auto t0 = chrono::high_resolution_clock::now();
    UnsignedInteger result;
    volatile bool vbool = false;
    volatile long long vll = 0;
    volatile double vd = 0;
    for (int i = 0; i < round; ++i) {
        if (op == "mul") result = a * b;
        else if (op == "mul2") result = a2 * a;
        else if (op == "sqr") {
#ifdef USE_ORIGINAL
            result = a * a;
#else
            result = a.square();
#endif
        }
        else if (op == "div") result = aBig / bSmall;
        else if (op == "div2") result = a2 / a;
        else if (op == "mod") result = aBig % bSmall;
        else if (op == "add") result = a + b;
        else if (op == "sub") result = aBig - bSmall;
        else if (op == "ctor") result = UnsignedInteger(sa.c_str());
        else if (op == "tostr") { const char* s = static_cast<const char*>(a); (void)s; }
        else if (op == "cmp") { vbool = (a == b) | (a != b) | (a < b) | (a > b) | (a <= b) | (a >= b); }
        else if (op == "conv") {
            vll = static_cast<long long>(a);
            vd = static_cast<double>(a);
            string s = static_cast<string>(a);
            (void)s;
        }
        else if (op == "io") {
            ostringstream oss;
            oss << a;
            istringstream iss(oss.str());
            iss >> result;
        }
    }
    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();
    cout << tag << "," << ms << endl;
#ifdef PROFILE_DIVISION
    printf("--- Division Profile (total over %d rounds) ---\n", round);
    profile_div::dump();
#endif
    (void)result; (void)vbool; (void)vll; (void)vd;
    return 0;
}
