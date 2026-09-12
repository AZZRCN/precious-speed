// 临时 runner: 用 CreateProcess + NUL 重定向运行 moptm_ADD_prof.exe
// 对齐 bench.cpp 的计时方式, 避免 Start-Process 管道阻塞
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv) {
    const char *exe = argc > 1 ? argv[1] : "moptm_ADD_prof.exe";
    const char *input = argc > 2 ? argv[2] : "test_add_1M_1M.txt";

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hInput = CreateFileA(input, GENERIC_READ, FILE_SHARE_READ, &sa,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInput == INVALID_HANDLE_VALUE) { printf("Cannot open %s\n", input); return 1; }

    HANDLE hNul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hNul == INVALID_HANDLE_VALUE) { printf("Cannot open NUL\n"); return 1; }

    HANDLE hErr = CreateFileA("prof_stderr.txt", GENERIC_WRITE, FILE_SHARE_WRITE, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hInput;
    si.hStdOutput = hNul;
    si.hStdError = hErr;

    PROCESS_INFORMATION pi = {};

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s", exe);
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (!ok) { printf("CreateProcess failed: %lu\n", GetLastError()); return 1; }

    WaitForSingleObject(pi.hProcess, 10000);

    QueryPerformanceCounter(&t1);
    double ms = double(t1.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);

    printf("Total (CreateProcess+NUL): %.2fms  exit=%lu\n", ms, code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hInput);
    CloseHandle(hNul);
    CloseHandle(hErr);
    return 0;
}
