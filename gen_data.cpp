// 通用大整数测试数据生成器
// 编译: g++ -O2 -o gen_data.exe gen_data.cpp
// 运行:
//   gen_data              -> 生成 add/mul/div 全部测试数据（默认种子 42）
//   gen_data add          -> 仅生成 ADD 数据
//   gen_data mul          -> 仅生成 MUL 数据
//   gen_data div          -> 仅生成 DIV 数据
//   gen_data div 12345    -> 仅生成 DIV 数据，用种子 12345
// 输出格式（三题统一）: "1\nA\nB\n"（test_count=1, A, B 各占一行）
#include <cstdio>
#include <cstring>
#include <random>

// 生成 n 位随机大整数（首位非 0），写入 f
static void genNum(std::mt19937 &rng, int n, FILE *f) {
    fputc(char('1' + rng() % 9), f); // 首位 1-9
    for (int i = 1; i < n; i++) {
        fputc(char('0' + rng() % 10), f);
    }
    fputc('\n', f);
}

// 生成一个测试用例文件
static void genCase(std::mt19937 &rng, const char *fname, int aN, int bN) {
    FILE *f = fopen(fname, "w");
    if (!f) { fprintf(stderr, "无法打开 %s\n", fname); return; }
    fprintf(f, "1\n");
    genNum(rng, aN, f);
    genNum(rng, bN, f);
    fclose(f);
    printf("  %s: A=%d, B=%d\n", fname, aN, bN);
}

// ADD 用例：两个相近规模的大数相加
static void genAdd(std::mt19937 &rng) {
    printf("[ADD]\n");
    genCase(rng, "test_add_100k_100k.txt",  100000, 100000);
    genCase(rng, "test_add_500k_500k.txt",  500000, 500000);
    genCase(rng, "test_add_1M_1M.txt",      1000000, 1000000);
}

// MUL 用例：两个相近规模的大数相乘（避免 1M*1M 太慢）
static void genMul(std::mt19937 &rng) {
    printf("[MUL]\n");
    genCase(rng, "test_mul_100k_100k.txt",  100000, 100000);
    genCase(rng, "test_mul_300k_300k.txt",  300000, 300000);
    genCase(rng, "test_mul_500k_500k.txt",  500000, 500000);
}

// DIV 用例：覆盖各种长度比（LC 计分 = 最长耗时点，重点关注大规模）
static void genDiv(std::mt19937 &rng) {
    printf("[DIV]\n");
    genCase(rng, "test_div_100k_10k.txt",   100000, 10000);
    genCase(rng, "test_div_200k_20k.txt",   200000, 20000);
    genCase(rng, "test_div_500k_50k.txt",   500000, 50000);
    genCase(rng, "test_div_500k_100k.txt",  500000, 100000);
    genCase(rng, "test_div_1M_100k.txt",    1000000, 100000);
    genCase(rng, "test_div_1M_500k.txt",    1000000, 500000);
    genCase(rng, "test_div_1M_900k.txt",    1000000, 900000);
    genCase(rng, "test_div_1M_999k.txt",    1000000, 999000);
}

int main(int argc, char **argv) {
    unsigned seed = 42;
    const char *mode = "all";
    if (argc >= 2) mode = argv[1];
    if (argc >= 3) seed = (unsigned)strtoul(argv[2], nullptr, 10);

    std::mt19937 rng(seed);
    printf("gen_data: mode=%s, seed=%u\n", mode, seed);

    if (!strcmp(mode, "all") || !strcmp(mode, "add")) genAdd(rng);
    if (!strcmp(mode, "all") || !strcmp(mode, "mul")) genMul(rng);
    if (!strcmp(mode, "all") || !strcmp(mode, "div")) genDiv(rng);

    printf("Done\n");
    return 0;
}
