// 可靠计时基准测试器 v2
// 特性:
//   - 三模式支持: bench add|mul|div|all
//   - 10 次运行取中位数（去最高最低后中位数）+ min/max/stddev
//   - 预热: 每个程序首次 2 次运行结果丢弃
//   - 5 秒超时保护（防止死锁）
//   - 退出码检测（非 0 视为失败）
//   - 自动编译: 检测源文件时间戳，过期则用 system() 调用 g++ 重编
//   - LC 计分: 所有测试点 median 的最大值
// 编译: g++ -O2 -o bench.exe bench.cpp
// 运行:
//   bench              -> 跑 add/mul/div 全部
//   bench div          -> 仅跑 DIV
//   bench div --rebuild -> 强制重编所有 exe
//   bench div --runs 15 -> 自定义运行次数（默认 10）
//   bench cmp div      -> 正确性验证: 对每个 DIV 用例跑 moptm vs best, 逐字节比对 stdout
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

// MSYS2 GMP 路径（编译/运行 GMP exe 需要）
static const char *MSYS_BIN = "C:\\msys64\\mingw64\\bin";
static const char *MSYS_INC = "C:\\msys64\\mingw64\\include";
static const char *MSYS_LIB = "C:\\msys64\\mingw64\\lib";

// 编译选项
static const char *MOPTM_FLAGS = "-O3 -mavx2 -mfma -funroll-loops";
static const char *BEST_FLAGS  = "-O3 -mavx2 -mfma -funroll-loops";
static const char *GMP_FLAGS   = "-O2";

static const int DEFAULT_RUNS = 10;
static const int WARMUP_RUNS = 2;
static const DWORD TIMEOUT_MS = 5000;

// ---------- 文件时间戳检测 ----------
static FILETIME fileMtime(const char *path) {
    FILETIME ft = {};
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return ft;
    GetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    return ft;
}

static bool fileExists(const char *path) {
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

// exe 是否比 src 新（无需重编）。任一不存在则需重编。
static bool exeFresh(const char *exe, const char **srcs, int n) {
    if (!fileExists(exe)) return false;
    FILETIME exe_ft = fileMtime(exe);
    for (int i = 0; i < n; i++) {
        FILETIME src_ft = fileMtime(srcs[i]);
        if (CompareFileTime(&src_ft, &exe_ft) > 0) return false;
    }
    return true;
}

// 编译并检查结果。返回 true 表示成功。
static bool compile(const char *cmd) {
    printf("  [compile] %s\n", cmd);
    int rc = system(cmd);
    if (rc != 0) {
        printf("  [compile] FAILED (rc=%d)\n", rc);
        return false;
    }
    return true;
}

// ---------- 自动编译所有 exe ----------
struct BuildTarget {
    const char *exe;
    std::vector<std::string> srcs;
    std::string cmd;
};

static bool buildAll(bool force) {
    printf("=== Build phase ===\n");

    // MSYS2 加到 PATH（编译 GMP + 运行 GMP exe 都需要）
    const char *old_path = getenv("PATH");
    if (old_path) {
        std::string new_path = std::string(MSYS_BIN) + ";" + old_path;
        _putenv_s("PATH", new_path.c_str());
    }

    bool ok = true;

    // moptm 三模式
    const char *moptm_srcs[] = {"moptm.cpp"};
    const char *ops[] = {"ADD", "MUL", "DIV"};
    for (int i = 0; i < 3; i++) {
        char exe[64], cmd[512];
        snprintf(exe, sizeof(exe), "moptm_%s.exe", ops[i]);
        snprintf(cmd, sizeof(cmd), "g++ %s -DHINT_OP_%s -o %s moptm.cpp 2>build_%s.err",
                 MOPTM_FLAGS, ops[i], exe, ops[i]);
        if (force || !exeFresh(exe, moptm_srcs, 1)) {
            if (!compile(cmd)) ok = false;
        } else {
            printf("  [skip] %s (fresh)\n", exe);
        }
    }

    // best 三模式
    const char *best_srcs[] = {"best\\add.cpp", "best\\mul.cpp", "best\\div.cpp"};
    const char *best_outs[] = {"best_add.exe", "best_mul.exe", "best_div.exe"};
    for (int i = 0; i < 3; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "g++ %s -o %s %s 2>build_%s.err",
                 BEST_FLAGS, best_outs[i], best_srcs[i], best_outs[i]);
        const char *srcs[] = {best_srcs[i]};
        if (force || !exeFresh(best_outs[i], srcs, 1)) {
            if (!compile(cmd)) ok = false;
        } else {
            printf("  [skip] %s (fresh)\n", best_outs[i]);
        }
    }

    // gmp 三模式
    const char *gmp_srcs[] = {"gmp_bench.cpp"};
    for (int i = 0; i < 3; i++) {
        char exe[64], cmd[512];
        snprintf(exe, sizeof(exe), "gmp_%s.exe", ops[i]);
        snprintf(cmd, sizeof(cmd),
                 "%s\\g++.exe %s -I%s -L%s -o %s gmp_bench.cpp -lgmp -DGMP_OP_%s 2>build_gmp_%s.err",
                 MSYS_BIN, GMP_FLAGS, MSYS_INC, MSYS_LIB, exe, ops[i], ops[i]);
        if (force || !exeFresh(exe, gmp_srcs, 1)) {
            if (!compile(cmd)) ok = false;
        } else {
            printf("  [skip] %s (fresh)\n", exe);
        }
    }

    printf("=== Build %s ===\n\n", ok ? "OK" : "FAILED");
    return ok;
}

// ---------- 计时核心 ----------
struct RunResult {
    double ms;
    int exitCode;
    bool timeout;
    bool failed;
};

// 运行一次 exe < inputFile > NUL，带超时。返回 RunResult。
static RunResult runOnce(const char *exe, const char *inputFile) {
    RunResult r = {-1.0, -1, false, false};

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hInput = CreateFileA(inputFile, GENERIC_READ, FILE_SHARE_READ, &sa,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInput == INVALID_HANDLE_VALUE) {
        r.failed = true;
        return r;
    }

    HANDLE hNul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hNul == INVALID_HANDLE_VALUE) {
        CloseHandle(hInput);
        r.failed = true;
        return r;
    }

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hInput;
    si.hStdOutput = hNul;
    si.hStdError = hNul;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s", exe);

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!ok) {
        CloseHandle(hInput);
        CloseHandle(hNul);
        r.failed = true;
        return r;
    }

    DWORD wait_rc = WaitForSingleObject(pi.hProcess, TIMEOUT_MS);
    QueryPerformanceCounter(&t1);
    r.ms = double(t1.QuadPart - t0.QuadPart) * 1000.0 / double(freq.QuadPart);

    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 99);
        r.timeout = true;
        r.failed = true;
    } else {
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        r.exitCode = (int)code;
        if (code != 0) r.failed = true;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hInput);
    CloseHandle(hNul);
    return r;
}

// ---------- 统计 ----------
struct Stats {
    double median;
    double min;
    double max;
    double mean;
    double stddev;
    int validRuns;
    bool failed;
};

// 对一个 (exe, input) 组合运行 warmup + N 次，返回统计。
// N 次结果排序后去最高最低，中位数 = (n-1)/2 位置（标准中位数定义）。
static Stats runBench(const char *exe, const char *inputFile, int runs) {
    Stats s = {0, 0, 0, 0, 0, 0, false};

    // 预热: 2 次，丢弃结果
    for (int i = 0; i < WARMUP_RUNS; i++) {
        RunResult r = runOnce(exe, inputFile);
        if (r.failed) {
            s.failed = true;
            return s;
        }
    }

    std::vector<double> samples;
    samples.reserve(runs);
    for (int i = 0; i < runs; i++) {
        RunResult r = runOnce(exe, inputFile);
        if (r.failed) {
            s.failed = true;
            return s;
        }
        samples.push_back(r.ms);
    }

    std::sort(samples.begin(), samples.end());
    int n = (int)samples.size();
    s.min = samples[0];
    s.max = samples[n - 1];
    // 中位数: 去掉最高 1 个和最低 1 个后取中位数（n>=3 时）
    if (n >= 4) {
        std::vector<double> trimmed(samples.begin() + 1, samples.end() - 1);
        int tn = (int)trimmed.size();
        if (tn % 2 == 0) {
            s.median = (trimmed[tn / 2 - 1] + trimmed[tn / 2]) / 2.0;
        } else {
            s.median = trimmed[tn / 2];
        }
        double sum = 0;
        for (double v : trimmed) sum += v;
        s.mean = sum / tn;
        double var = 0;
        for (double v : trimmed) var += (v - s.mean) * (v - s.mean);
        s.stddev = sqrt(var / tn);
    } else {
        s.median = samples[n / 2];
        s.mean = s.median;
        s.stddev = 0;
    }
    s.validRuns = n;
    return s;
}

// ---------- 测试用例定义 ----------
struct TestCase {
    const char *name;
    const char *file;
};

struct Program {
    const char *name;
    const char *exe;
};

static const TestCase ADD_CASES[] = {
    {"100k+100k", "test_add_100k_100k.txt"},
    {"500k+500k", "test_add_500k_500k.txt"},
    {"1M+1M",     "test_add_1M_1M.txt"},
};
static const TestCase MUL_CASES[] = {
    {"100k*100k", "test_mul_100k_100k.txt"},
    {"300k*300k", "test_mul_300k_300k.txt"},
    {"500k*500k", "test_mul_500k_500k.txt"},
};
static const TestCase DIV_CASES[] = {
    {"100k/10k",  "test_div_100k_10k.txt"},
    {"200k/20k",  "test_div_200k_20k.txt"},
    {"500k/50k",  "test_div_500k_50k.txt"},
    {"500k/100k", "test_div_500k_100k.txt"},
    {"1M/100k",   "test_div_1M_100k.txt"},
    {"1M/500k",   "test_div_1M_500k.txt"},
    {"1M/900k",   "test_div_1M_900k.txt"},
    {"1M/999k",   "test_div_1M_999k.txt"},
};

// 跑一个 op 模式
static void runMode(const char *op, const TestCase *cases, int nCases, int runs) {
    char gmp_exe[32], best_exe[32], moptm_exe[32];
    snprintf(gmp_exe,   sizeof(gmp_exe),   "gmp_%s.exe",   op);
    snprintf(best_exe,  sizeof(best_exe),  "best_%s.exe",  op);
    snprintf(moptm_exe, sizeof(moptm_exe), "moptm_%s.exe", op);

    Program progs[3] = {
        {"GMP",   gmp_exe},
        {"best",  best_exe},
        {"moptm", moptm_exe},
    };

    printf("=== Bench %s (runs=%d, warmup=%d) ===\n", op, runs, WARMUP_RUNS);
    printf("%-12s %22s %22s %22s %12s %12s\n",
           "case", "GMP (med/min/max)", "best (med/min/max)",
           "moptm (med/min/max)", "moptm/GMP", "moptm/best");
    printf("--------------------------------------------------------------------------------------------------------------\n");

    double maxMoptm = 0, maxBest = 0, maxGmp = 0;
    for (int i = 0; i < nCases; i++) {
        printf("%-12s ", cases[i].name);
        double results[3] = {0, 0, 0};
        bool any_failed = false;
        for (int p = 0; p < 3; p++) {
            Stats s = runBench(progs[p].exe, cases[i].file, runs);
            if (s.failed) {
                printf("%22s ", "FAIL");
                any_failed = true;
                continue;
            }
            results[p] = s.median;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f/%.1f/%.1f", s.median, s.min, s.max);
            printf("%22s ", buf);
        }
        double g = results[0], b = results[1], m = results[2];
        if (m > maxMoptm) maxMoptm = m;
        if (b > maxBest)  maxBest = b;
        if (g > maxGmp)   maxGmp = g;
        if (any_failed || g <= 0 || b <= 0) {
            printf("%12s %12s\n", "-", "-");
        } else {
            printf("%11.2fx %11.2fx\n", m / g, m / b);
        }
    }
    printf("--------------------------------------------------------------------------------------------------------------\n");
    printf("%-12s %22s %22.1f %22.1f %11.2fx %11.2fx\n",
           "LC score", "(max of medians)", maxBest, maxMoptm,
           maxGmp > 0 ? maxMoptm / maxGmp : 0.0,
           maxBest > 0 ? maxMoptm / maxBest : 0.0);
    printf("\n");
}

// ---------- 正确性验证 (cmp 模式) ----------
// 运行 exe < inputFile > outputFile，带 10 秒超时。返回退出码（-1=超时/失败）。
static int runCapture(const char *exe, const char *inputFile, const char *outputFile) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    HANDLE hInput = CreateFileA(inputFile, GENERIC_READ, FILE_SHARE_READ, &sa,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInput == INVALID_HANDLE_VALUE) return -2;

    HANDLE hOutput = CreateFileA(outputFile, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutput == INVALID_HANDLE_VALUE) {
        CloseHandle(hInput);
        return -3;
    }

    HANDLE hNul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hInput;
    si.hStdOutput = hOutput;
    si.hStdError = hNul;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s", exe);

    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!ok) {
        CloseHandle(hInput);
        CloseHandle(hOutput);
        if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
        return -4;
    }

    DWORD wait_rc = WaitForSingleObject(pi.hProcess, 10000); // 10s 超时
    DWORD code = 0;
    if (wait_rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 99);
        code = (DWORD)-1;
    } else {
        GetExitCodeProcess(pi.hProcess, &code);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hInput);
    CloseHandle(hOutput);
    if (hNul != INVALID_HANDLE_VALUE) CloseHandle(hNul);
    return (int)code;
}

// 逐字节比较两个文件。返回 true=相同。
static bool compareFiles(const char *f1, const char *f2, size_t &diffOffset) {
    diffOffset = 0;
    HANDLE h1 = CreateFileA(f1, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE h2 = CreateFileA(f2, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h1 == INVALID_HANDLE_VALUE || h2 == INVALID_HANDLE_VALUE) {
        if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
        if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
        return false;
    }

    LARGE_INTEGER sz1, sz2;
    GetFileSizeEx(h1, &sz1);
    GetFileSizeEx(h2, &sz2);
    if (sz1.QuadPart != sz2.QuadPart) {
        CloseHandle(h1);
        CloseHandle(h2);
        diffOffset = SIZE_MAX;
        return false;
    }

    // 64KB 缓冲区逐块比较
    const size_t BUF = 65536;
    static unsigned char buf1[BUF], buf2[BUF];
    size_t offset = 0;
    while (offset < (size_t)sz1.QuadPart) {
        DWORD toRead = (DWORD)((sz1.QuadPart - offset < (LONGLONG)BUF) ? (sz1.QuadPart - offset) : BUF);
        DWORD r1 = 0, r2 = 0;
        if (!ReadFile(h1, buf1, toRead, &r1, NULL) || r1 != toRead) break;
        if (!ReadFile(h2, buf2, toRead, &r2, NULL) || r2 != toRead) break;
        if (memcmp(buf1, buf2, toRead) != 0) {
            for (DWORD i = 0; i < toRead; i++) {
                if (buf1[i] != buf2[i]) {
                    diffOffset = offset + i;
                    CloseHandle(h1);
                    CloseHandle(h2);
                    return false;
                }
            }
        }
        offset += toRead;
    }

    CloseHandle(h1);
    CloseHandle(h2);
    return true;
}

// 验证一个 op 模式: 对每个用例跑 moptm vs best，逐字节比对 stdout
static void verifyMode(const char *op, const TestCase *cases, int nCases) {
    char moptm_exe[32], best_exe[32];
    snprintf(moptm_exe, sizeof(moptm_exe), "moptm_%s.exe", op);
    snprintf(best_exe,  sizeof(best_exe),  "best_%s.exe",  op);

    printf("=== Verify %s: moptm vs best (byte-by-byte) ===\n", op);
    printf("%-16s %12s %12s %10s %s\n", "case", "moptm_rc", "best_rc", "result", "detail");
    printf("-----------------------------------------------------------------\n");

    int pass = 0, fail = 0;
    for (int i = 0; i < nCases; i++) {
        printf("%-16s ", cases[i].name);
        fflush(stdout);

        int rc_m = runCapture(moptm_exe, cases[i].file, "out_moptm.txt");
        int rc_b = runCapture(best_exe,  cases[i].file, "out_best.txt");

        if (rc_m != 0 || rc_b != 0) {
            printf("%12d %12d %10s\n", rc_m, rc_b, "RUNFAIL");
            fail++;
            continue;
        }

        size_t diffOff = 0;
        bool same = compareFiles("out_moptm.txt", "out_best.txt", diffOff);
        if (same) {
            printf("%12d %12d %10s\n", rc_m, rc_b, "PASS");
            pass++;
        } else {
            char detail[64];
            if (diffOff == SIZE_MAX) {
                // 文件大小不同，读取两个文件大小
                HANDLE h1 = CreateFileA("out_moptm.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                HANDLE h2 = CreateFileA("out_best.txt", GENERIC_READ, FILE_SHARE_READ, NULL,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                LARGE_INTEGER s1, s2;
                GetFileSizeEx(h1, &s1);
                GetFileSizeEx(h2, &s2);
                CloseHandle(h1);
                CloseHandle(h2);
                snprintf(detail, sizeof(detail), "size mismatch %lld vs %lld",
                         (long long)s1.QuadPart, (long long)s2.QuadPart);
            } else {
                snprintf(detail, sizeof(detail), "diff at byte %zu", diffOff);
            }
            printf("%12d %12d %10s %s\n", rc_m, rc_b, "FAIL", detail);
            fail++;
        }
    }
    printf("-----------------------------------------------------------------\n");
    printf("Result: %d passed, %d failed (mode=%s)\n\n", pass, fail, op);
}

int main(int argc, char **argv) {
    std::string mode = "all";
    std::string cmp_op = "";
    bool rebuild = false;
    int runs = DEFAULT_RUNS;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--rebuild")) rebuild = true;
        else if (!strcmp(argv[i], "--runs") && i + 1 < argc) {
            runs = atoi(argv[++i]);
            if (runs < 4) runs = DEFAULT_RUNS;
        } else if (mode == "cmp") {
            // 第二个非 flag 参数视为 cmp 的 op (add/mul/div)
            cmp_op = argv[i];
        } else {
            mode = argv[i];
        }
    }

    // 编译
    if (!buildAll(rebuild)) {
        printf("Build failed, continue anyway (some tests may fail)...\n\n");
    }

    // 检查测试数据
    if (!fileExists("test_add_100k_100k.txt") || !fileExists("test_div_1M_500k.txt")) {
        printf("Test data missing, running gen_data...\n");
        int rc = system("gen_data.exe");
        if (rc != 0) {
            printf("gen_data.exe failed (rc=%d). Compile it first:\n", rc);
            printf("  g++ -O2 -o gen_data.exe gen_data.cpp\n");
            return 1;
        }
        printf("\n");
    }

    // cmp 模式: 正确性验证
    if (mode == "cmp") {
        if (cmp_op == "add" || cmp_op == "all") {
            verifyMode("add", ADD_CASES, sizeof(ADD_CASES) / sizeof(ADD_CASES[0]));
        }
        if (cmp_op == "mul" || cmp_op == "all") {
            verifyMode("mul", MUL_CASES, sizeof(MUL_CASES) / sizeof(MUL_CASES[0]));
        }
        if (cmp_op == "div" || cmp_op == "all") {
            verifyMode("div", DIV_CASES, sizeof(DIV_CASES) / sizeof(DIV_CASES[0]));
        }
        if (cmp_op.empty()) {
            verifyMode("add", ADD_CASES, sizeof(ADD_CASES) / sizeof(ADD_CASES[0]));
            verifyMode("mul", MUL_CASES, sizeof(MUL_CASES) / sizeof(MUL_CASES[0]));
            verifyMode("div", DIV_CASES, sizeof(DIV_CASES) / sizeof(DIV_CASES[0]));
        }
        return 0;
    }

    if (mode == "all" || mode == "add") {
        runMode("add", ADD_CASES, sizeof(ADD_CASES) / sizeof(ADD_CASES[0]), runs);
    }
    if (mode == "all" || mode == "mul") {
        runMode("mul", MUL_CASES, sizeof(MUL_CASES) / sizeof(MUL_CASES[0]), runs);
    }
    if (mode == "all" || mode == "div") {
        runMode("div", DIV_CASES, sizeof(DIV_CASES) / sizeof(DIV_CASES[0]), runs);
    }

    if (mode != "all" && mode != "add" && mode != "mul" && mode != "div") {
        printf("Unknown mode: %s\n", mode.c_str());
        printf("Usage: bench [add|mul|div|all|cmp [op]] [--rebuild] [--runs N]\n");
        printf("  cmp [op]  - verify moptm vs best (op=add|mul|div|all, default all)\n");
        return 1;
    }
    return 0;
}
