// comprehensive_test.cpp — 原版 vs 优化版全方位正确性对比
// 编译:
//   g++ -O2 -std=c++20 -DUSE_ORIGINAL -o ctest_orig.exe comprehensive_test.cpp
//   g++ -O2 -std=c++20 -o ctest_opt.exe  comprehensive_test.cpp
// 运行: ./ctest_orig.exe > orig.txt ; ./ctest_opt.exe > opt.txt
// 对比: diff orig.txt opt.txt  (应无差异)
//
// 设计原则:
//   1. 分模块: 构造/赋值/转换/比较/算术(无符号)/算术(有符号)/I/O/square/边界规模/大规模
//   2. 大批量: 每个模块多组输入, 覆盖各种规模和边界值
//   3. 穿插测试: main 中交替调用不同模块, 模拟真实使用模式
//   4. 指纹输出: 每个用例输出 "TAG|fingerprint", 便于 diff 对比
//   5. 公平性: square() 测试在原版用 x*x 代替, 优化版用 x.square(), 结果应一致

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <sstream>

#ifdef USE_ORIGINAL
#include "../masonxiong.cpp"
#elif defined(USE_EXPERIMENTAL)
#include "masonxiong_EXPERIMENTAL.cpp"
#else
#include "masonxiong_opt.cpp"
#endif

using namespace std;

// ==================== 工具函数 ====================

// 确定性随机数字串生成 (固定 seed, 保证两版输入相同)
string genDigits(size_t n, unsigned seed) {
    string s(n, '0');
    srand(seed);
    s[0] = '1' + (rand() % 9);
    for (size_t i = 1; i < n; ++i) s[i] = '0' + (rand() % 10);
    return s;
}

// 带符号随机串
string genSignedDigits(size_t n, unsigned seed) {
    string s = genDigits(n, seed);
    if (seed % 2 == 0) s = "-" + s;
    return s;
}

// 无符号整数指纹: 小数全输出, 大数输出 头16...尾16(长度)
string uiFP(const UnsignedInteger& x) {
    string s = static_cast<string>(x);
    if (s.size() <= 40) return s;
    return s.substr(0, 16) + "..." + s.substr(s.size() - 16) + "(" + to_string(s.size()) + ")";
}

// 有符号整数指纹: 带符号
string siFP(const SignedInteger& x) {
    string s = static_cast<string>(x);
    if (s.size() <= 40) return s;
    return s.substr(0, 16) + "..." + s.substr(s.size() - 16) + "(" + to_string(s.size()) + ")";
}

// 输出宏
#define OUT(tag, fp) cout << tag << "|" << fp << "\n"

// ==================== 模块1: 构造函数 ====================

void testCtorUI() {
    // 默认构造
    OUT("CTOR_UI_DEFAULT", uiFP(UnsignedInteger()));
    // 字符串构造 - 各种规模
    OUT("CTOR_UI_S0", uiFP(UnsignedInteger("0")));
    OUT("CTOR_UI_S1", uiFP(UnsignedInteger("1")));
    OUT("CTOR_UI_S7", uiFP(UnsignedInteger("1234567")));
    OUT("CTOR_UI_S8", uiFP(UnsignedInteger("12345678")));
    OUT("CTOR_UI_S9", uiFP(UnsignedInteger("123456789")));
    OUT("CTOR_UI_S16", uiFP(UnsignedInteger("1234567890123456")));
    OUT("CTOR_UI_S17", uiFP(UnsignedInteger("12345678901234567")));
    // 整数构造
    OUT("CTOR_UI_U0", uiFP(UnsignedInteger(0u)));
    OUT("CTOR_UI_U1", uiFP(UnsignedInteger(1u)));
    OUT("CTOR_UI_U_BASE", uiFP(UnsignedInteger(100000000u)));
    OUT("CTOR_UI_U_BASE_M1", uiFP(UnsignedInteger(99999999u)));
    OUT("CTOR_UI_U_BIG", uiFP(UnsignedInteger(4294967295u)));
    OUT("CTOR_UI_I0", uiFP(UnsignedInteger(0)));
    OUT("CTOR_UI_I12345", uiFP(UnsignedInteger(12345)));
    OUT("CTOR_UI_I_MAX", uiFP(UnsignedInteger(2147483647)));
    // 浮点构造
    OUT("CTOR_UI_D0", uiFP(UnsignedInteger(0.0)));
    OUT("CTOR_UI_D1", uiFP(UnsignedInteger(1.5)));
    OUT("CTOR_UI_D_BASE", uiFP(UnsignedInteger(1e8)));
    OUT("CTOR_UI_D_BIG", uiFP(UnsignedInteger(1.23456789e15)));
    // std::string 构造
    OUT("CTOR_UI_STR", uiFP(UnsignedInteger(string("9876543210"))));
    // 拷贝/移动构造
    UnsignedInteger a("12345678901234567890");
    UnsignedInteger b(a);
    OUT("CTOR_UI_COPY", uiFP(b));
    UnsignedInteger c(move(b));
    OUT("CTOR_UI_MOVE", uiFP(c));
    // 字面量
    OUT("CTOR_UI_LITERAL", uiFP("999999999999999999999"_UI));
    // 大规模字符串构造
    string big32 = genDigits(32, 100);
    string big64 = genDigits(64, 101);
    string big128 = genDigits(128, 102);
    string big256 = genDigits(256, 103);
    OUT("CTOR_UI_S32", uiFP(UnsignedInteger(big32.c_str())));
    OUT("CTOR_UI_S64", uiFP(UnsignedInteger(big64.c_str())));
    OUT("CTOR_UI_S128", uiFP(UnsignedInteger(big128.c_str())));
    OUT("CTOR_UI_S256", uiFP(UnsignedInteger(big256.c_str())));
}

void testCtorSI() {
    OUT("CTOR_SI_DEFAULT", siFP(SignedInteger()));
    OUT("CTOR_SI_POS", siFP(SignedInteger("123456789")));
    OUT("CTOR_SI_NEG", siFP(SignedInteger("-123456789")));
    OUT("CTOR_SI_ZERO", siFP(SignedInteger("0")));
    OUT("CTOR_SI_U", siFP(SignedInteger(UnsignedInteger("42"))));
    OUT("CTOR_SI_I_POS", siFP(SignedInteger(12345)));
    OUT("CTOR_SI_I_NEG", siFP(SignedInteger(-12345)));
    OUT("CTOR_SI_I_ZERO", siFP(SignedInteger(0)));
    OUT("CTOR_SI_D", siFP(SignedInteger(-3.14)));
    OUT("CTOR_SI_STR", siFP(SignedInteger(string("-9876543210"))));
    SignedInteger a("-12345678901234567890");
    SignedInteger b(a);
    OUT("CTOR_SI_COPY", siFP(b));
    SignedInteger c(move(b));
    OUT("CTOR_SI_MOVE", siFP(c));
    OUT("CTOR_SI_LITERAL", siFP("-999999999999999999999"_SI));
    // 大规模
    string big128 = genSignedDigits(128, 200);
    OUT("CTOR_SI_S128", siFP(SignedInteger(big128.c_str())));
}

// ==================== 模块2: 赋值 ====================

void testAssignUI() {
    UnsignedInteger x;
    x = "1234567890";
    OUT("ASSIGN_UI_CSTR", uiFP(x));
    x = string("9876543210");
    OUT("ASSIGN_UI_STR", uiFP(x));
    x = 42u;
    OUT("ASSIGN_UI_UINT", uiFP(x));
    x = 999;
    OUT("ASSIGN_UI_INT", uiFP(x));
    x = 3.14;
    OUT("ASSIGN_UI_DOUBLE", uiFP(x));
    UnsignedInteger a("12345678901234567890");
    UnsignedInteger b;
    b = a;
    OUT("ASSIGN_UI_COPY", uiFP(b));
    b = move(a);
    OUT("ASSIGN_UI_MOVE", uiFP(b));
}

void testAssignSI() {
    SignedInteger x;
    x = "-1234567890";
    OUT("ASSIGN_SI_CSTR", siFP(x));
    x = string("9876543210");
    OUT("ASSIGN_SI_STR", siFP(x));
    x = 42;
    OUT("ASSIGN_SI_INT", siFP(x));
    x = -999;
    OUT("ASSIGN_SI_NEG", siFP(x));
    x = 3.14;
    OUT("ASSIGN_SI_DOUBLE", siFP(x));
    SignedInteger a("-12345678901234567890");
    SignedInteger b;
    b = a;
    OUT("ASSIGN_SI_COPY", siFP(b));
    b = move(a);
    OUT("ASSIGN_SI_MOVE", siFP(b));
    b = UnsignedInteger("42");
    OUT("ASSIGN_SI_FROM_UI", siFP(b));
}

// ==================== 模块3: 类型转换 ====================

void testConvUI() {
    UnsignedInteger a("0");
    OUT("CONV_UI_BOOL_0", to_string(static_cast<bool>(a)));
    UnsignedInteger b("12345");
    OUT("CONV_UI_BOOL_1", to_string(static_cast<bool>(b)));
    OUT("CONV_UI_U64", to_string(static_cast<unsigned long long>(b)));
    OUT("CONV_UI_I64", to_string(static_cast<long long>(b)));
    OUT("CONV_UI_DOUBLE", to_string(static_cast<double>(b)));
    OUT("CONV_UI_STR", static_cast<string>(b));
    {
        const char* p = static_cast<const char*>(b);
        OUT("CONV_UI_CSTR", string(p));
    }
    // 大数转整数 (截断)
    UnsignedInteger c("999999999999999999999");
    OUT("CONV_UI_BIG_U64", to_string(static_cast<unsigned long long>(c)));
    OUT("CONV_UI_BIG_I64", to_string(static_cast<long long>(c)));
    OUT("CONV_UI_BIG_DBL", to_string(static_cast<double>(c)));
}

void testConvSI() {
    SignedInteger a("0");
    OUT("CONV_SI_BOOL_0", to_string(static_cast<bool>(a)));
    SignedInteger b("-12345");
    OUT("CONV_SI_BOOL_1", to_string(static_cast<bool>(b)));
    OUT("CONV_SI_I64", to_string(static_cast<long long>(b)));
    OUT("CONV_SI_U64", to_string(static_cast<unsigned long long>(b)));
    OUT("CONV_SI_DOUBLE", to_string(static_cast<double>(b)));
    OUT("CONV_SI_STR", static_cast<string>(b));
    {
        const char* p = static_cast<const char*>(b);
        OUT("CONV_SI_CSTR", string(p));
    }
}

// ==================== 模块4: 比较 ====================

void testCmpUI() {
    vector<pair<string, string>> pairs = {
        {"0", "0"},
        {"0", "1"},
        {"1", "0"},
        {"12345", "12345"},
        {"12345", "12346"},
        {"12346", "12345"},
        {"99999999", "100000000"},
        {"100000000", "99999999"},
        {"12345678901234567890", "12345678901234567890"},
        {"12345678901234567890", "12345678901234567891"},
        {"12345678901234567891", "12345678901234567890"},
        {genDigits(100, 300), genDigits(100, 301)},
        {genDigits(100, 301), genDigits(100, 300)},
        {genDigits(100, 300), genDigits(100, 300)},
    };
    for (size_t i = 0; i < pairs.size(); ++i) {
        UnsignedInteger a(pairs[i].first.c_str()), b(pairs[i].second.c_str());
        stringstream ss;
        ss << (a == b) << (a != b) << (a < b) << (a > b) << (a <= b) << (a >= b);
        OUT("CMP_UI_" + to_string(i), ss.str());
    }
    // bool 转换
    OUT("CMP_UI_BOOL0", to_string(bool(UnsignedInteger("0"))));
    OUT("CMP_UI_BOOL1", to_string(bool(UnsignedInteger("1"))));
}

void testCmpSI() {
    vector<pair<string, string>> pairs = {
        {"0", "0"}, {"0", "1"}, {"1", "0"},
        {"12345", "12345"}, {"12345", "-12345"}, {"-12345", "12345"},
        {"-12345", "-12346"}, {"-12346", "-12345"},
        {"12345678901234567890", "-12345678901234567890"},
        {genSignedDigits(100, 400), genSignedDigits(100, 401)},
        {genSignedDigits(100, 401), genSignedDigits(100, 400)},
        {genSignedDigits(100, 400), genSignedDigits(100, 400)},
    };
    for (size_t i = 0; i < pairs.size(); ++i) {
        SignedInteger a(pairs[i].first.c_str()), b(pairs[i].second.c_str());
        stringstream ss;
        ss << (a == b) << (a != b) << (a < b) << (a > b) << (a <= b) << (a >= b);
        OUT("CMP_SI_" + to_string(i), ss.str());
    }
}

// ==================== 模块5: 算术 (无符号) ====================

// 辅助: 对一组 (a,b) 执行所有算术运算并输出指纹
void arithUIPair(const string& tag, const char* sa, const char* sb) {
    UnsignedInteger a(sa), b(sb);
    // 加法
    { UnsignedInteger r = a + b; OUT(tag + "_ADD", uiFP(r)); }
    // 减法 (仅 a >= b 时合法)
    if (!(a < b)) {
        UnsignedInteger r = a - b; OUT(tag + "_SUB", uiFP(r));
    }
    // 乘法
    { UnsignedInteger r = a * b; OUT(tag + "_MUL", uiFP(r)); }
    // 除法 (b != 0)
    if (b) {
        UnsignedInteger q = a / b; OUT(tag + "_DIV", uiFP(q));
        UnsignedInteger m = a % b; OUT(tag + "_MOD", uiFP(m));
    }
    // 自乘 (优化版走 square() 短路, 原版走普通乘法)
    { UnsignedInteger r = a * a; OUT(tag + "_SELFMUL", uiFP(r)); }
    // square() — 原版无此方法, 用 x*x 代替; 优化版用专用路径
#ifdef USE_ORIGINAL
    { UnsignedInteger r = a * a; OUT(tag + "_SQUARE", uiFP(r)); }
#else
    { UnsignedInteger r = a.square(); OUT(tag + "_SQUARE", uiFP(r)); }
#endif
    // 复合赋值
    { UnsignedInteger r(a); r += b; OUT(tag + "_ADDEQ", uiFP(r)); }
    if (!(a < b)) {
        { UnsignedInteger r(a); r -= b; OUT(tag + "_SUBEQ", uiFP(r)); }
    }
    { UnsignedInteger r(a); r *= b; OUT(tag + "_MULEQ", uiFP(r)); }
    if (b) {
        { UnsignedInteger r(a); r /= b; OUT(tag + "_DIVEQ", uiFP(r)); }
        { UnsignedInteger r(a); r %= b; OUT(tag + "_MODEQ", uiFP(r)); }
    }
    // 自增自减
    { UnsignedInteger r(a); ++r; OUT(tag + "_PREINC", uiFP(r)); }
    { UnsignedInteger r(a); r++; OUT(tag + "_POSTINC", uiFP(r)); }
    if (a) {
        { UnsignedInteger r(a); --r; OUT(tag + "_PREDEC", uiFP(r)); }
        { UnsignedInteger r(a); r--; OUT(tag + "_POSTDEC", uiFP(r)); }
    }
}

void testArithUISmall() {
    // 小规模 (bruteforce 范围, < 128 digit)
    arithUIPair("ARITH_UI_S_0", "0", "0");
    arithUIPair("ARITH_UI_S_1", "1", "0");
    arithUIPair("ARITH_UI_S_2", "0", "1");
    arithUIPair("ARITH_UI_S_3", "12345", "6789");
    arithUIPair("ARITH_UI_S_4", "99999999", "1");
    arithUIPair("ARITH_UI_S_5", "100000000", "1");
    arithUIPair("ARITH_UI_S_6", "99999999999999999999", "1");
    arithUIPair("ARITH_UI_S_7", "12345678901234567890", "98765432109876543210");
    arithUIPair("ARITH_UI_S_8", genDigits(32, 500).c_str(), genDigits(32, 501).c_str());
    arithUIPair("ARITH_UI_S_9", genDigits(48, 502).c_str(), genDigits(48, 503).c_str());
    arithUIPair("ARITH_UI_S_10", genDigits(60, 504).c_str(), genDigits(60, 505).c_str());
}

void testArithUIBoundary() {
    // BruteforceThreshold 边界 (原版 64, 优化版 128)
    // 这些规模会触发不同的内部路径
    arithUIPair("ARITH_UI_B_64", genDigits(64, 510).c_str(), genDigits(64, 511).c_str());
    arithUIPair("ARITH_UI_B_96", genDigits(96, 512).c_str(), genDigits(96, 513).c_str());
    arithUIPair("ARITH_UI_B_128", genDigits(128, 514).c_str(), genDigits(128, 515).c_str());
    arithUIPair("ARITH_UI_B_160", genDigits(160, 516).c_str(), genDigits(160, 517).c_str());
    arithUIPair("ARITH_UI_B_200", genDigits(200, 518).c_str(), genDigits(200, 519).c_str());
    arithUIPair("ARITH_UI_B_256", genDigits(256, 520).c_str(), genDigits(256, 521).c_str());
}

void testArithUIMedium() {
    // 中规模 (FFT 范围)
    arithUIPair("ARITH_UI_M_512", genDigits(512, 530).c_str(), genDigits(512, 531).c_str());
    arithUIPair("ARITH_UI_M_1K", genDigits(1024, 532).c_str(), genDigits(1024, 533).c_str());
    arithUIPair("ARITH_UI_M_2K", genDigits(2048, 534).c_str(), genDigits(2048, 535).c_str());
    arithUIPair("ARITH_UI_M_4K", genDigits(4096, 536).c_str(), genDigits(4096, 537).c_str());
}

void testArithUILarge() {
    // 大规模 (不平衡乘法拆分触发: a.length >= 2*b.length)
    {
        string big = genDigits(10000, 540);
        string small = genDigits(1000, 541);
        arithUIPair("ARITH_UI_L_10K_1K", big.c_str(), small.c_str());
    }
    {
        // 不平衡: 10K x 100
        string big = genDigits(10000, 542);
        string small = genDigits(100, 543);
        arithUIPair("ARITH_UI_L_10K_100", big.c_str(), small.c_str());
    }
    {
        // 大规模除法 (Newton 迭代)
        string big = genDigits(10000, 544);
        string small = genDigits(10000, 545);
        arithUIPair("ARITH_UI_L_10K_10K", big.c_str(), small.c_str());
    }
    {
        // 100K 规模
        string big = genDigits(100000, 546);
        string small = genDigits(100000, 547);
        arithUIPair("ARITH_UI_L_100K", big.c_str(), small.c_str());
    }
}

// ==================== 模块6: 算术 (有符号) ====================

void arithSIPair(const string& tag, const char* sa, const char* sb) {
    SignedInteger a(sa), b(sb);
    { SignedInteger r = a + b; OUT(tag + "_ADD", siFP(r)); }
    { SignedInteger r = a - b; OUT(tag + "_SUB", siFP(r)); }
    { SignedInteger r = a * b; OUT(tag + "_MUL", siFP(r)); }
    if (b) {
        { SignedInteger r = a / b; OUT(tag + "_DIV", siFP(r)); }
        { SignedInteger r = a % b; OUT(tag + "_MOD", siFP(r)); }
    }
    { SignedInteger r(a); r += b; OUT(tag + "_ADDEQ", siFP(r)); }
    { SignedInteger r(a); r -= b; OUT(tag + "_SUBEQ", siFP(r)); }
    { SignedInteger r(a); r *= b; OUT(tag + "_MULEQ", siFP(r)); }
    if (b) {
        { SignedInteger r(a); r /= b; OUT(tag + "_DIVEQ", siFP(r)); }
        { SignedInteger r(a); r %= b; OUT(tag + "_MODEQ", siFP(r)); }
    }
}

void testArithSI() {
    arithSIPair("ARITH_SI_0", "0", "0");
    arithSIPair("ARITH_SI_1", "12345", "6789");
    arithSIPair("ARITH_SI_2", "-12345", "6789");
    arithSIPair("ARITH_SI_3", "12345", "-6789");
    arithSIPair("ARITH_SI_4", "-12345", "-6789");
    arithSIPair("ARITH_SI_5", "99999999", "1");
    arithSIPair("ARITH_SI_6", "12345678901234567890", "98765432109876543210");
    arithSIPair("ARITH_SI_7", "-12345678901234567890", "98765432109876543210");
    arithSIPair("ARITH_SI_8", genSignedDigits(256, 600).c_str(), genSignedDigits(256, 601).c_str());
    arithSIPair("ARITH_SI_9", genSignedDigits(1024, 602).c_str(), genSignedDigits(1024, 603).c_str());
    arithSIPair("ARITH_SI_10", genSignedDigits(4096, 604).c_str(), genSignedDigits(4096, 605).c_str());
}

// ==================== 模块7: I/O ====================

void testIO() {
    // ostream
    {
        ostringstream oss;
        oss << UnsignedInteger("12345678901234567890");
        OUT("IO_OUT_UI", oss.str());
    }
    {
        ostringstream oss;
        oss << SignedInteger("-12345678901234567890");
        OUT("IO_OUT_SI", oss.str());
    }
    // istream
    {
        istringstream iss("9876543210");
        UnsignedInteger x;
        iss >> x;
        OUT("IO_IN_UI", uiFP(x));
    }
    {
        istringstream iss("-9876543210");
        SignedInteger x;
        iss >> x;
        OUT("IO_IN_SI", siFP(x));
    }
    // 大数 I/O 往返
    {
        string big = genDigits(1000, 700);
        ostringstream oss;
        oss << UnsignedInteger(big.c_str());
        istringstream iss(oss.str());
        UnsignedInteger x;
        iss >> x;
        OUT("IO_ROUNDTRIP_1K", uiFP(x));
    }
}

// ==================== 模块8: 大规模正确性 (精简, 只测关键路径) ====================

void testLargeScale() {
    // 1M 自乘 (square 路径)
    {
        string big = genDigits(1000000, 800);
        UnsignedInteger a(big.c_str());
#ifdef USE_ORIGINAL
        UnsignedInteger r = a * a;
#else
        UnsignedInteger r = a.square();
#endif
        OUT("LARGE_SQR_1M", uiFP(r));
    }
    // 1M 乘法
    {
        string a_str = genDigits(1000000, 801);
        string b_str = genDigits(1000000, 802);
        UnsignedInteger a(a_str.c_str()), b(b_str.c_str());
        UnsignedInteger r = a * b;
        OUT("LARGE_MUL_1M", uiFP(r));
    }
    // 1M 除法 (a^2 / a, 应得 a)
    {
        string big = genDigits(1000000, 803);
        UnsignedInteger a(big.c_str());
        UnsignedInteger a2 = a * a;
        UnsignedInteger r = a2 / a;
        OUT("LARGE_DIV2_1M", uiFP(r));
    }
    // 不平衡乘法 100K x 10K
    {
        string big = genDigits(100000, 804);
        string small = genDigits(10000, 805);
        UnsignedInteger a(big.c_str()), b(small.c_str());
        UnsignedInteger r = a * b;
        OUT("LARGE_UNBAL_100K_10K", uiFP(r));
    }
}

// ==================== main: 穿插执行 ====================

int main() {
    // 穿插测试: 不同模块交替执行, 模拟真实使用模式
    // 模块间穿插: 小规模 → 中规模 → 构造 → 比较 → 边界 → 转换 → 大规模 → I/O → 有符号

    testCtorUI();
    testArithUISmall();
    testCtorSI();
    testCmpUI();
    testArithUIBoundary();
    testAssignUI();
    testConvUI();
    testArithUIMedium();
    testCmpSI();
    testIO();
    testAssignSI();
    testConvSI();
    testArithSI();
    testArithUILarge();
    testLargeScale();

    cout.flush();
    return 0;
}
