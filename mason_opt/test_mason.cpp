// test_mason.cpp - masonxiong_opt.cpp 版本（参考实现，正确性基线）
// 编译: g++ -O3 -mavx2 -mfma -funroll-loops -o test_mason.exe test_mason.cpp
// 运行: test_mason.exe [input_file] [output_file]
#include "masonxiong_opt.cpp"
#include <fstream>

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::istream* in = &std::cin;
    std::ostream* out = &std::cout;
    std::ifstream fin;
    std::ofstream fout;
    if (argc > 1) { fin.open(argv[1]); in = &fin; }
    if (argc > 2) { fout.open(argv[2]); out = &fout; }
    int t;
    *in >> t;
    UnsignedInteger a, b;
    while (t--) {
        *in >> a >> b;
        auto res = a.divisionAndModulus(b);
        *out << res.first << " " << res.second << "\n";
    }
    if (fout.is_open()) fout.flush();
    return 0;
}
