// test_work.cpp - div_work.cpp 版本
// 编译: g++ -O3 -mavx2 -mfma -funroll-loops -o test_work.exe test_work.cpp
// 运行: test_work.exe < input.txt
#define main unused_main
#include "div_work.cpp"
#undef main

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int t;
    std::cin >> t;
    hint::Integer a, b, q, r;
    while (t--) {
        std::cin >> a >> b;
        a.absDivRem(b, q, r);
        std::cout << q << " " << r << "\n";
    }
    return 0;
}
