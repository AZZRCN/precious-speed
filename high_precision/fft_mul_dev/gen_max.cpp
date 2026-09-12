// gen_max.cpp —— 造官方上限规模 (HEX LOG_16 = 1.6e6 hex digits) 的 div 用例
// 用法: ./gen_max <na_hexdigits> <nb_hexdigits> <seed> > out.in
// 输出格式: 第一行 Q=1, 第二行 A, 第三行 B (与 cases/*.in 同格式)
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: gen_max na nb seed\n"); return 1; }
    const size_t na = strtoull(argv[1], nullptr, 10);
    const size_t nb = strtoull(argv[2], nullptr, 10);
    uint64_t st = strtoull(argv[3], nullptr, 10) * 6364136223846793005ull + 1442695040888963407ull;
    auto nxt = [&]() -> uint64_t {          // splitmix64
        uint64_t z = (st += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    };
    static const char HX[] = "0123456789abcdef";
    std::vector<char> buf;

    auto emit = [&](size_t n) {
        buf.clear(); buf.resize(n + 1);
        buf[0] = HX[1 + (nxt() % 15)];      // 首位非零
        for (size_t i = 1; i != n; ++i) buf[i] = HX[nxt() & 15];
        buf[n] = '\n';
        fwrite(buf.data(), 1, n + 1, stdout);
    };

    fputs("1\n", stdout);
    emit(na);
    emit(nb);
    return 0;
}
