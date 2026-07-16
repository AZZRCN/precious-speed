// div_check.cpp - 模板化对拍程序
// 用法: div_check.exe <cases> <na> <nb> <seed> <tag>
// 它会:
//   1. 调用 gen_div_input.exe 生成输入
//   2. 运行两个测试程序（通过宏配置），用 system() 重定向
//   3. 逐行对比输出
// 编译: g++ -O2 -o div_check.exe div_check.cpp
//
// 通过 -DPROG_A / -DPROG_B 指定两个待测程序路径。
//   g++ -O2 -DPROG_A=\"test_work.exe\" -DPROG_B=\"test_mason.exe\" -o div_check.exe div_check.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#ifndef PROG_A
#define PROG_A "test_work.exe"
#endif
#ifndef PROG_B
#define PROG_B "test_mason.exe"
#endif
#ifndef GEN_PROG
#define GEN_PROG "gen_div_input.exe"
#endif

// 读文件全部行
static std::vector<std::string> readLines(const char* path) {
    std::ifstream fin(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fin, line)) {
        // 去掉 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <cases> <na> <nb> <seed> [tag]\n", argv[0]);
        return 2;
    }
    int cases = atoi(argv[1]);
    int na = atoi(argv[2]);
    int nb = atoi(argv[3]);
    unsigned seed = (unsigned)atoi(argv[4]);
    const char* tag = argc > 5 ? argv[5] : "";

    // 1. 生成输入
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd), "%s %d %d %d %u > input.txt 2>nul",
                  GEN_PROG, cases, na, nb, seed);
    int rc = system(cmd);
    if (rc != 0) {
        printf("[%s] GEN FAIL rc=%d\n", tag, rc);
        return 2;
    }

    // 2. 运行 A
    std::snprintf(cmd, sizeof(cmd), "%s < input.txt > out_a.txt 2>nul", PROG_A);
    rc = system(cmd);
    if (rc != 0) {
        printf("[%s] PROG_A FAIL rc=%d\n", tag, rc);
        return 2;
    }

    // 3. 运行 B
    std::snprintf(cmd, sizeof(cmd), "%s < input.txt > out_b.txt 2>nul", PROG_B);
    rc = system(cmd);
    if (rc != 0) {
        printf("[%s] PROG_B FAIL rc=%d\n", tag, rc);
        return 2;
    }

    // 4. 对比输出
    auto la = readLines("out_a.txt");
    auto lb = readLines("out_b.txt");

    if (la.size() != lb.size()) {
        printf("[%s] FAIL: line count %zu vs %zu\n", tag, la.size(), lb.size());
        // 打印前 3 行差异
        size_t n = std::min(la.size(), lb.size());
        int shown = 0;
        for (size_t i = 0; i < n && shown < 3; i++) {
            if (la[i] != lb[i]) {
                printf("  line %zu:\n    A: %.80s\n    B: %.80s\n",
                       i + 1, la[i].c_str(), lb[i].c_str());
                shown++;
            }
        }
        return 1;
    }

    int fails = 0;
    for (size_t i = 0; i < la.size(); i++) {
        if (la[i] != lb[i]) {
            fails++;
            if (fails <= 3) {
                printf("  line %zu:\n    A: %.80s\n    B: %.80s\n",
                       i + 1, la[i].c_str(), lb[i].c_str());
            }
        }
    }
    if (fails == 0) {
        printf("[%s] PASS (%d cases, na=%d nb=%d seed=%u)\n", tag, cases, na, nb, seed);
        return 0;
    } else {
        printf("[%s] FAIL: %d / %zu lines\n", tag, fails, la.size());
        return 1;
    }
}
