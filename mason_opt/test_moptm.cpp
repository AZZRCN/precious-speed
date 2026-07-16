// test_moptm.cpp - moptm.cpp version (hybrid division), file I/O for fair benchmarking
// Compile: g++ -O2 -mavx2 -mfma -funroll-loops -o test_moptm.exe test_moptm.cpp
#define MOPTM_NO_IO
#include "moptm.cpp"

#include <fstream>

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::istream* in = &std::cin;
    std::ostream* out = &std::cout;
    std::ifstream fin;
    std::ofstream fout;
    if (argc > 1) {
        fin.open(argv[1]);
        if (!fin.is_open()) { std::cerr << "cannot open input: " << argv[1] << "\n"; return 1; }
        in = &fin;
    }
    if (argc > 2) {
        fout.open(argv[2]);
        if (!fout.is_open()) { std::cerr << "cannot open output: " << argv[2] << "\n"; return 1; }
        out = &fout;
    }
    int t;
    *in >> t;
    if (in->fail()) { std::cerr << "read t failed, argc=" << argc << "\n"; return 1; }
    UnsignedInteger a, b;
    while (t--) {
        *in >> a >> b;
        auto res = a.divisionAndModulus(b);
        *out << res.first << " " << res.second << "\n";
    }
    if (fout.is_open()) fout.flush();
    return 0;
}
