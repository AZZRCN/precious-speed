// bench_mul.cpp - file I/O multiplication benchmark
// Compile: g++ -O2 -mavx2 -mfma -funroll-loops -o bench_mul.exe bench_mul.cpp
#define MOPTM_NO_IO
#include "moptm.cpp"
#include <fstream>

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    if (argc < 3) { std::cerr << "usage: bench_mul.exe <input> <output>\n"; return 1; }
    std::ifstream fin(argv[1]);
    std::ofstream fout(argv[2]);
    if (!fin.is_open() || !fout.is_open()) { std::cerr << "cannot open files\n"; return 1; }
    int t;
    fin >> t;
    SignedInteger a, b;
    while (t--) {
        fin >> a >> b;
        a *= b;
        fout << a << "\n";
    }
    fout.flush();
    return 0;
}
