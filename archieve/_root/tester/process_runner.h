#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <thread>

struct ProcessResult {
    bool ok = false;
    std::string out;
    std::string err;
    int exitCode = -1;
    DWORD ms = 0;
    bool timedOut = false;
};

inline ProcessResult runProcess(const std::string& exePath,
                                const std::string& args,
                                const std::string& input,
                                DWORD timeoutMs = 30000) {
    ProcessResult result;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdoutRead = nullptr;
    HANDLE hStdoutWrite = nullptr;
    HANDLE hStderrRead = nullptr;
    HANDLE hStderrWrite = nullptr;
    HANDLE hStdinRead = nullptr;
    HANDLE hStdinWrite = nullptr;

    do {
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 1 << 20)) break;
        if (!SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0)) break;
        if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) break;
        if (!SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0)) break;
        if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 1 << 20)) break;
        if (!SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0)) break;

        std::string cmd = "\"" + exePath + "\"";
        if (!args.empty()) cmd += " " + args;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
        std::wstring wcmd(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, wcmd.data(), wlen);

        STARTUPINFOW si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput  = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError  = hStderrWrite;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        LARGE_INTEGER freq, t0, t1;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);

        if (!CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) break;

        CloseHandle(hStdinRead);   hStdinRead  = nullptr;
        CloseHandle(hStdoutWrite); hStdoutWrite = nullptr;
        CloseHandle(hStderrWrite); hStderrWrite = nullptr;

        // Transfer ownership of hStdinWrite to the writer thread
        HANDLE hWrite = hStdinWrite;
        hStdinWrite = nullptr;

        // Writer thread: write stdin data, then close handle (EOF signal)
        std::thread writeThread([hWrite, &input]() {
            if (!input.empty()) {
                const char* p = input.data();
                size_t remaining = input.size();
                while (remaining > 0) {
                    DWORD toWrite = (DWORD)(std::min)(remaining, (size_t)0x10000);
                    DWORD wr = 0;
                    if (!WriteFile(hWrite, p, toWrite, &wr, nullptr)) break;
                    p += wr;
                    remaining -= wr;
                }
            }
            CloseHandle(hWrite);
        });

        // Reader thread: read stdout concurrently (avoids blocking main thread on timeout)
        std::thread readOutThread([&result, hStdoutRead]() {
            char buf[0x10000];
            DWORD nRead = 0;
            while (ReadFile(hStdoutRead, buf, sizeof(buf), &nRead, nullptr) && nRead > 0) {
                result.out.append(buf, nRead);
            }
        });

        // Reader thread: read stderr concurrently
        std::thread readErrThread([&result, hStderrRead]() {
            char buf[0x10000];
            DWORD nRead = 0;
            while (ReadFile(hStderrRead, buf, sizeof(buf), &nRead, nullptr) && nRead > 0) {
                result.err.append(buf, nRead);
            }
        });

        // Main thread: wait for process with timeout
        DWORD waitMs = (timeoutMs > 0) ? timeoutMs : INFINITE;
        DWORD waitRes = WaitForSingleObject(pi.hProcess, waitMs);
        QueryPerformanceCounter(&t1);
        result.ms = (DWORD)((t1.QuadPart - t0.QuadPart) * 1000 / freq.QuadPart);

        if (waitRes == WAIT_TIMEOUT) {
            result.timedOut = true;
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 2000);
        }

        DWORD ec = 0;
        GetExitCodeProcess(pi.hProcess, &ec);
        result.exitCode = (int)ec;
        result.ok = !result.timedOut && (ec == 0);

        // Close write end so reader threads can finish (writeThread already closes it)
        // Join all threads
        writeThread.join();
        readOutThread.join();
        readErrThread.join();

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } while (false);

    if (hStdoutRead)  CloseHandle(hStdoutRead);
    if (hStdoutWrite) CloseHandle(hStdoutWrite);
    if (hStderrRead)  CloseHandle(hStderrRead);
    if (hStderrWrite) CloseHandle(hStderrWrite);
    if (hStdinRead)   CloseHandle(hStdinRead);
    if (hStdinWrite)  CloseHandle(hStdinWrite);
    return result;
}
