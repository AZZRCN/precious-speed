// div_check_moptm.cpp - 对拍程序: test_moptm vs test_mason
// 编译: g++ -O2 -o div_check_moptm.exe div_check_moptm.cpp
// 用法: div_check_moptm.exe <cases> <na> <nb> <seed> [tag]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

static std::vector<std::string> readLines(const char* path) {
    std::ifstream fin(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fin, line)) {
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

    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd), "gen_div_input.exe %d %d %d %u > input.txt 2>nul",
                  cases, na, nb, seed);
    if (system(cmd) != 0) { printf("[%s] GEN FAIL\n", tag); return 2; }

    std::snprintf(cmd, sizeof(cmd), "test_moptm.exe < input.txt > out_a.txt 2>nul");
    if (system(cmd) != 0) { printf("[%s] PROG_A(moptm) FAIL\n", tag); return 2; }

    std::snprintf(cmd, sizeof(cmd), "test_mason.exe < input.txt > out_b.txt 2>nul");
    if (system(cmd) != 0) { printf("[%s] PROG_B(mason) FAIL\n", tag); return 2; }

    auto la = readLines("out_a.txt");
    auto lb = readLines("out_b.txt");
    if (la.size() != lb.size()) {
        printf("[%s] FAIL: line count %zu vs %zu\n", tag, la.size(), lb.size());
        int shown = 0;
        for (size_t i = 0; i < std::min(la.size(), lb.size()) && shown < 3; i++) {
            if (la[i] != lb[i]) {
                printf("  line %zu:\n    moptm: %.80s\n    mason: %.80s\n",
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
                printf("  line %zu:\n    moptm: %.80s\n    mason: %.80s\n",
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
