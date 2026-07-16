// verify.cpp — 用 Python 交叉验证 sqr/div2 正确性
#include <iostream>
#include <string>
#ifdef USE_ORIGINAL
#include "../masonxiong.cpp"
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
    size_t scale = argc > 1 ? atoi(argv[1]) : 1000000;
    string op = argc > 2 ? argv[2] : "sqr";

    string sa = genRandomDigits(scale, 42);
    string sb = genRandomDigits(scale, 43);

    UnsignedInteger a(sa.c_str()), b(sb.c_str());
    UnsignedInteger a2 = a * a;
    UnsignedInteger aBig = a > b ? a : b;
    UnsignedInteger bSmall = a > b ? b : a;

    UnsignedInteger result;
    if (op == "sqr") result = a * a;
    else if (op == "div2") result = a2 / a;
    else if (op == "mul") result = a * b;
    else if (op == "div") result = aBig / bSmall;
    else { cerr << "unknown op: " << op << endl; return 1; }

    // 输出结果的字符串形式的前 20 和后 20 位，以及长度
    string rs = static_cast<string>(result);
    cout << "op=" << op << " scale=" << scale << " len=" << rs.size() << endl;
    if (rs.size() <= 40) {
        cout << "result=" << rs << endl;
    } else {
        cout << "head=" << rs.substr(0, 20) << endl;
        cout << "tail=" << rs.substr(rs.size() - 20) << endl;
    }
    return 0;
}
