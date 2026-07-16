// div_check2_all.cpp - 通用对拍驱动（带超时版）
// 编译: g++ -O2 -o div_check2_all.exe div_check2_all.cpp
// 用法: div_check2_all.exe <progA> <progB>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Case {
    const char* tag;
    int cases;
    int na;
    int nb;
    unsigned seed;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <progA> <progB>\n", argv[0]);
        return 2;
    }
    const char* progA = argv[1];
    const char* progB = argv[2];

    Case cases[] = {
        {"small_eq",      50,     10,     10,    42},
        {"small_neq",     50,     20,     10,    43},
        {"mid_eq",        20,   1000,   1000,   44},
        {"mid_neq",       20,   2000,   1000,   45},
        {"large_eq",       5,  10000,  10000,   46},
        {"large_neq",      5,  20000,  10000,   47},
        {"huge_eq",        2, 100000, 100000,   48},
        {"huge_neq",       2, 200000, 100000,   49},
        {"div2",           3, 200000, 100000,   50},
        {"div4",           2, 400000, 100000,   51},
        {"div8",           1, 800000, 100000,   52},
        {"tiny_b",        30,    100,      2,   53},
        {"edge_a_eq_b",   10,     50,     50,   54},
        {"edge_a_lt_b",   10,     10,     50,   55},
        {"boundary_b1",   20,    100,      1,   56},
        {"core2_div2",     5,  50000,  25000,   60},
        {"core2_div3",     3,  90000,  30000,   61},
        {"core2_big",      1, 1000000, 100000,  62},  // blocks=10
    };
    int n = sizeof(cases) / sizeof(cases[0]);
    int pass = 0, fail = 0;
    for (int i = 0; i < n; i++) {
        char cmd[1024];
        std::snprintf(cmd, sizeof(cmd), "div_check2.exe %d %d %d %u %s %s %s",
                      cases[i].cases, cases[i].na, cases[i].nb, cases[i].seed,
                      progA, progB, cases[i].tag);
        int rc = system(cmd);
        if (rc == 0) pass++;
        else fail++;
    }
    printf("\n========== %d PASS / %d FAIL / %d TOTAL ==========\n", pass, fail, n);
    return fail == 0 ? 0 : 1;
}
