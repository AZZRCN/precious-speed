// bench_moptm_all.cpp - moptm vs masonxiong_opt 性能对比驱动
// 编译: g++ -O2 -o bench_moptm_all.exe bench_moptm_all.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cmath>

struct Case {
    const char* tag;
    int round;
    int na;
    int nb;
};

struct Stats {
    double sum, mean, med, trim, min, max;
    std::vector<double> vals;
};

static Stats computeStats(std::vector<double>& v) {
    std::sort(v.begin(), v.end());
    Stats s;
    s.vals = v;
    s.sum = 0; for (double x : v) s.sum += x;
    s.mean = s.sum / v.size();
    s.min = v[0];
    s.max = v.back();
    s.med = (v[v.size()/2 - 1] + v[v.size()/2]) / 2.0;
    // trim: 去掉前后各 20%
    int trimCnt = v.size() / 5;
    double trimSum = 0;
    int trimN = 0;
    for (int i = trimCnt; i < (int)v.size() - trimCnt; i++) { trimSum += v[i]; trimN++; }
    s.trim = trimSum / trimN;
    return s;
}

int main() {
    Case cases[] = {
        {"large_00",   5, 100000, 100000,},  // len1≈len2，Core1
        {"large_01",   5, 200000, 100000,},  // blocks=2
        {"ratio_int",  3, 400000, 100000,},  // blocks=4，length_ratio_integer
        {"ratio_big",  2, 800000, 100000,},  // blocks=8
        {"a_max_b",    3, 200000, 200000,},  // a_max_b_random
        {"huge_eq",    2, 500000, 500000,},  // 大等长
    };
    int n = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < n; i++) {
        printf("\n=== %s (na=%d nb=%d round=%d) ===\n", cases[i].tag, cases[i].na, cases[i].nb, cases[i].round);

        std::vector<double> moptmTimes, masonTimes;
        for (int r = 0; r < 10; r++) {
            char cmd[512];
            // moptm
            std::snprintf(cmd, sizeof(cmd), "bench_moptm.exe %d %d %d M%d",
                          cases[i].round, cases[i].na, cases[i].nb, r);
            FILE* fp = popen(cmd, "r");
            if (fp) {
                char buf[256];
                if (fgets(buf, sizeof(buf), fp)) {
                    char* p = strchr(buf, ',');
                    if (p) moptmTimes.push_back(atof(p + 1));
                }
                pclose(fp);
            }
            // mason
            std::snprintf(cmd, sizeof(cmd), "bench_mason.exe %d %d %d M%d",
                          cases[i].round, cases[i].na, cases[i].nb, r);
            fp = popen(cmd, "r");
            if (fp) {
                char buf[256];
                if (fgets(buf, sizeof(buf), fp)) {
                    char* p = strchr(buf, ',');
                    if (p) masonTimes.push_back(atof(p + 1));
                }
                pclose(fp);
            }
        }

        if (moptmTimes.size() < 10 || masonTimes.size() < 10) {
            printf("  ERROR: not enough data (moptm=%zu mason=%zu)\n", moptmTimes.size(), masonTimes.size());
            continue;
        }

        Stats ms = computeStats(moptmTimes);
        Stats mn = computeStats(masonTimes);

        printf("MOPTM: sum=%.1f mean=%.1f med=%.1f trim=%.1f min=%.1f max=%.1f\n",
               ms.sum, ms.mean, ms.med, ms.trim, ms.min, ms.max);
        printf("MASON: sum=%.1f mean=%.1f med=%.1f trim=%.1f min=%.1f max=%.1f\n",
               mn.sum, mn.mean, mn.med, mn.trim, mn.min, mn.max);
        printf("RATIO: trim=%.4f (delta %.1f%%)  min=%.4f (delta %.1f%%)\n",
               ms.trim / mn.trim, (ms.trim / mn.trim - 1) * 100,
               ms.min / mn.min, (ms.min / mn.min - 1) * 100);
    }
    return 0;
}
