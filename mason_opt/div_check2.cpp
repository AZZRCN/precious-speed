// div_check2.cpp - 带超时的对拍程序（文件 I/O，无重定向）
// 编译: g++ -O2 -o div_check2.exe div_check2.cpp
// 用法: div_check2.exe <cases> <na> <nb> <seed> <progA> <progB> [tag]
//   progA/progB 接受 input_file output_file 两个参数
//   超时 15 秒自动杀死进程
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

static const DWORD TIMEOUT_MS = 15000;

// 运行程序: prog input_file output_file。超时返回 -1。
static int runWithTimeout(const char* prog, const char* inFile, const char* outFile, DWORD timeoutMs) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    char cmdLine[2048];
    std::snprintf(cmdLine, sizeof(cmdLine), "%s %s %s", prog, inFile, outFile);

    BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) return -3;

    DWORD rc = WaitForSingleObject(pi.hProcess, timeoutMs);
    int retVal;
    if (rc == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 99);
        WaitForSingleObject(pi.hProcess, 2000);
        retVal = -1;
    } else if (rc == WAIT_OBJECT_0) {
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        retVal = (int)exitCode;
    } else {
        retVal = -4;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return retVal;
}

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
    if (argc < 7) {
        fprintf(stderr, "usage: %s <cases> <na> <nb> <seed> <progA> <progB> [tag]\n", argv[0]);
        return 2;
    }
    int cases = atoi(argv[1]);
    int na = atoi(argv[2]);
    int nb = atoi(argv[3]);
    unsigned seed = (unsigned)atoi(argv[4]);
    const char* progA = argv[5];
    const char* progB = argv[6];
    const char* tag = argc > 7 ? argv[7] : "";

    // 1. 生成输入
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd), "gen_div_input.exe %d %d %d %u input.txt",
                  cases, na, nb, seed);
    if (system(cmd) != 0) { printf("[%s] GEN FAIL\n", tag); return 2; }

    // 用带 PID 的唯一文件名避免残留句柄锁定
    char fileA[64], fileB[64];
    std::snprintf(fileA, sizeof(fileA), "out_a_%lu.txt", GetCurrentProcessId());
    std::snprintf(fileB, sizeof(fileB), "out_b_%lu.txt", GetCurrentProcessId());

    // 2. 运行 A
    int rcA = runWithTimeout(progA, "input.txt", fileA, TIMEOUT_MS);
    if (rcA != 0) {
        printf("[%s] PROG_A %s FAIL rc=%d%s\n", tag, progA, rcA, rcA == -1 ? " (TIMEOUT)" : "");
        return 2;
    }

    // 3. 运行 B
    int rcB = runWithTimeout(progB, "input.txt", fileB, TIMEOUT_MS);
    if (rcB != 0) {
        printf("[%s] PROG_B %s FAIL rc=%d%s\n", tag, progB, rcB, rcB == -1 ? " (TIMEOUT)" : "");
        return 2;
    }

    // 4. 对比
    auto la = readLines(fileA);
    auto lb = readLines(fileB);
    DeleteFileA(fileA);
    DeleteFileA(fileB);
    if (la.size() != lb.size()) {
        printf("[%s] FAIL: line count %zu vs %zu\n", tag, la.size(), lb.size());
        int shown = 0;
        for (size_t i = 0; i < std::min(la.size(), lb.size()) && shown < 3; i++) {
            if (la[i] != lb[i]) {
                printf("  line %zu:\n    A: %.80s\n    B: %.80s\n", i + 1, la[i].c_str(), lb[i].c_str());
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
                printf("  line %zu:\n    A: %.80s\n    B: %.80s\n", i + 1, la[i].c_str(), lb[i].c_str());
            }
        }
    }
    if (fails == 0) {
        printf("[%s] PASS (%d cases, na=%d nb=%d)\n", tag, cases, na, nb);
        return 0;
    } else {
        printf("[%s] FAIL: %d / %zu lines\n", tag, fails, la.size());
        return 1;
    }
}
