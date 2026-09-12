// FFT 原语 microbench: 精确测 rdif / ridit / rdot / real_conv 的 Ir
// 用法: fftbench <float_len> <reps> <op>
//   op = dif | idit | dot | conv
// 配合 callgrind: 跑 reps=R 与 reps=1, 差分/(R-1) 得单次代价
#ifndef SRC_MAIN
#define SRC_MAIN "best/div_D16.cpp"
#endif
#define main hint_orig_main_disabled
#include SRC_MAIN
#undef main

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

using hint::transform::fft::rdif;
using hint::transform::fft::ridit;
using hint::transform::fft::rdot;
using hint::transform::fft::real_conv;

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s <float_len> <reps> <op>\n", argv[0]); return 2; }
    size_t fl = strtoull(argv[1], nullptr, 10);
    int reps = atoi(argv[2]);
    std::string op = argv[3];

    double *a = nullptr, *b = nullptr;
    if (posix_memalign((void **)&a, 32, fl * sizeof(double))) return 3;
    if (posix_memalign((void **)&b, 32, fl * sizeof(double))) return 3;
    // 固定伪随机填充 (幅度小, 避免 inf/nan)
    unsigned long long s = 88172645463325252ull;
    auto nx = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return double((s >> 11) & 8191) - 4096.0; };
    for (size_t i = 0; i < fl; i++) { a[i] = nx(); b[i] = nx(); }

    // 预热: 让 twiddle 表 expand 完成 (不计入差分, 但保证 reps=1 也已 warm)
    {
        double *w1 = nullptr, *w2 = nullptr;
        posix_memalign((void **)&w1, 32, fl * sizeof(double));
        posix_memalign((void **)&w2, 32, fl * sizeof(double));
        memcpy(w1, a, fl * sizeof(double));
        memcpy(w2, b, fl * sizeof(double));
        real_conv(w1, w2, fl);
        free(w1); free(w2);
    }

    double sink = 0;
    for (int r = 0; r < reps; r++)
    {
        if (op == "dif")       { rdif(a, fl); }
        else if (op == "idit") { ridit(a, fl); }
        else if (op == "dot")  { rdot(a, b, fl); }
        else if (op == "conv") { real_conv(a, b, fl); }
        else { fprintf(stderr, "bad op\n"); return 2; }
        // 防止被优化掉。注意: 不做任何数据相关的重填/守卫,
        // 否则每 rep 代价不恒定, 差分法失效。FFT 指令数与数据无关(无数据分支),
        // 数值溢出到 inf 不影响 Ir 计数。
        sink += a[0] + a[fl / 2];
    }
    fprintf(stderr, "sink=%g\n", sink);
    free(a); free(b);
    return 0;
}
