// gen_div_input.cpp - 生成除法测试输入数据
// 编译: g++ -O2 -o gen_div_input.exe gen_div_input.cpp
// 运行: gen_div_input.exe <cases> <na> <nb> <seed> [outfile]
//   不指定 outfile 时输出到 stdout
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc, char* argv[]) {
    int cases = atoi(argv[1]);
    int na = atoi(argv[2]);
    int nb = atoi(argv[3]);
    unsigned seed = argc > 4 ? atoi(argv[4]) : 42;
    FILE* out = stdout;
    if (argc > 5) {
        out = fopen(argv[5], "w");
        if (!out) { fprintf(stderr, "cannot open %s\n", argv[5]); return 1; }
    }
    srand(seed);
    fprintf(out, "%d\n", cases);
    for (int c = 0; c < cases; c++) {
        for (int i = 0; i < na; i++) {
            fputc(i == 0 ? ('1' + rand() % 9) : ('0' + rand() % 10), out);
        }
        fputc('\n', out);
        for (int i = 0; i < nb; i++) {
            fputc(i == 0 ? ('1' + rand() % 9) : ('0' + rand() % 10), out);
        }
        fputc('\n', out);
    }
    if (out != stdout) fclose(out);
    return 0;
}
