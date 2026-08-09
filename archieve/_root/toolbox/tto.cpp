// tto.cpp - Timeout Executor (超时执行器)
// 用法: tto <seconds> <command> [args...]
// 编译: g++ -O2 -std=c++17 -o tto.exe tto.cpp
// 超时后 kill 进程树，返回 124；正常返回子进程退出码
//
// 设计要点:
//   - CreateProcess 继承父进程 stdin/stdout/stderr (输出直通控制台)
//   - WaitForSingleObject 带超时等待
//   - 超时后 taskkill /F /T /PID 杀死整个进程树 (含子进程, 如 g++ 调用的 cc1plus)
//   - 不重定向输出, 避免 PowerShell redirection issues

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

static void print_usage() {
    fprintf(stderr,
        "tto - Timeout Executor\n"
        "\n"
        "Usage: tto <seconds> <command> [args...]\n"
        "  seconds   超时秒数 (1-3600)\n"
        "  command   要执行的程序\n"
        "  args      传递给程序的参数\n"
        "\n"
        "Exit code: 子进程退出码, 或 124 (超时), 或 1 (参数错误)\n"
        "Example: tto 30 g++ -O3 div.cpp -o div.exe\n"
        "         tto 10 cur_div.exe < input.txt\n");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    int timeoutSec = atoi(argv[1]);
    if (timeoutSec <= 0 || timeoutSec > 3600) {
        fprintf(stderr, "tto: invalid timeout '%s' (must be 1-3600)\n", argv[1]);
        return 1;
    }

    // 构建命令行: "program" arg1 arg2 ...
    // 给程序路径加引号, 处理路径中带空格的情况
    std::string cmdLine = "\"";
    cmdLine += argv[2];
    cmdLine += "\"";
    for (int i = 3; i < argc; i++) {
        cmdLine += " ";
        cmdLine += argv[i];
    }

    // 复制到可修改的缓冲区 (CreateProcessA 要求 LPSTR, 非 const)
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    // 创建进程, 继承父进程的标准句柄
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessA(
        NULL,               // lpApplicationName (NULL = 从命令行解析)
        cmdBuf.data(),      // lpCommandLine (可修改缓冲区)
        NULL,               // lpProcessAttributes
        NULL,               // lpThreadAttributes
        TRUE,               // bInheritHandles (继承标准句柄)
        0,                  // dwCreationFlags
        NULL,               // lpEnvironment (NULL = 继承父进程)
        NULL,               // lpCurrentDirectory (NULL = 继承父进程)
        &si,                // lpStartupInfo
        &pi                 // lpProcessInformation
    );

    if (!ok) {
        DWORD err = GetLastError();
        fprintf(stderr, "tto: CreateProcess failed (err=%lu): %s\n", err, cmdLine.c_str());
        // 常见错误码提示
        if (err == 2) {
            fprintf(stderr, "tto: program not found, check PATH or full path\n");
        }
        return 1;
    }

    // 带超时等待
    DWORD waitResult = WaitForSingleObject(pi.hProcess, (DWORD)timeoutSec * 1000);

    if (waitResult == WAIT_TIMEOUT) {
        // 超时: kill 整个进程树 (g++ 会 spawn cc1plus/as/ld 等子进程)
        char killCmd[256];
        _snprintf(killCmd, sizeof(killCmd),
                  "taskkill /F /T /PID %lu >NUL 2>&1", pi.dwProcessId);
        system(killCmd);
        // 确保进程句柄关闭
        WaitForSingleObject(pi.hProcess, 1000); // 等待 kill 完成
        fprintf(stderr, "\ntto: TIMEOUT after %ds (killed PID %lu): %s\n",
                timeoutSec, pi.dwProcessId, cmdLine.c_str());
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 124;
    }

    // 获取子进程退出码
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (int)exitCode;
}
