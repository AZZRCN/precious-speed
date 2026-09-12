// main.cpp - Big Integer Differential Tester (MSVC C++17)
// Usage: tester.exe [targetTotal] [threads] [quickMode]
//   quickMode > 0: limit seeds per generator to min(seedCount, quickMode)
#include "process_runner.h"
#include "thread_pool.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <direct.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
static const char* GEN_DIR = "tester\\gen";
static const char* EXE_DIR = "tester";
static const char* FAIL_DIR = "tester\\failures";

struct GenConfig {
    const char* genName;   // generator exe name (no path)
    const char* op;        // "add" / "mul" / "div"
    int seedCount;         // how many seeds (0..seedCount-1)
    DWORD timeoutMs;       // timeout for ref and cur
};

// Seed counts tuned for official LC generators:
// - carry_chain only supports seeds 0-3 (seed/2 must be 0 or 1)
// - Large cases have fewer seeds (slow reference solution)
// - Feature data (fft_killer, burnikel_ziegler) emphasized
static const GenConfig kConfigs[] = {
    // ADD (7 generators)
    {"add_small",        "add", 1000, 30000},
    {"add_medium",       "add",  800, 30000},
    {"add_large",        "add",  200, 60000},
    {"add_max_max",      "add",   10, 180000},
    {"add_carry_chain",  "add",    4, 60000},  // LC info.toml: number=4
    {"add_sum_zero",     "add",  200, 30000},
    {"add_large_small",  "add",  200, 30000},
    // MUL (7 generators)
    {"mul_small",        "mul", 1000, 30000},
    {"mul_medium",       "mul",  800, 30000},
    {"mul_large",        "mul",  200, 60000},
    {"mul_max_max",      "mul",   10, 180000},
    {"mul_fft_killer",   "mul",  200, 120000},
    {"mul_zero",         "mul",  200, 30000},
    {"mul_large_small",  "mul",  200, 30000},
    // DIV (8 generators)
    {"div_small",                  "div", 1000, 30000},
    {"div_medium",                 "div",  800, 30000},
    {"div_large",                  "div",  200, 60000},
    {"div_max",                    "div",    5, 600000},  // 200万字数字，correct.cpp慢，10分钟超时
    {"div_a_max_b_random",         "div",  100, 120000},
    {"div_burnikel_ziegler",       "div",  100, 120000},
    {"div_r_nearly_zero",          "div",  100, 120000},
    {"div_length_ratio_integer",   "div",  100, 60000},
};
static const int kConfigCount = sizeof(kConfigs) / sizeof(kConfigs[0]);

// ---------------------------------------------------------------------------
// Utils
// ---------------------------------------------------------------------------
static std::string exePath(const char* name) {
    return std::string(EXE_DIR) + "\\" + name + ".exe";
}
static std::string genExePath(const char* name) {
    return std::string(GEN_DIR) + "\\" + name + ".exe";
}

static std::string normalizeLineEndings(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c != '\r') out.push_back(c);
    }
    return out;
}

// Mutex for stderr output (avoid garbled multi-thread messages)
static std::mutex g_errMtx;
// Mutex for saving failure files
static std::mutex g_failMtx;
static int g_failIdx = 0;

static void saveFailure(const std::string& op, const std::string& genName, int seed,
                        const std::string& input,
                        const std::string& bestOut, const std::string& curOut) {
    std::lock_guard<std::mutex> lk(g_failMtx);
    int idx = g_failIdx++;
    std::ostringstream oss;
    oss << FAIL_DIR << "\\" << op << "_" << genName << "_" << seed << "_" << idx;
    std::string base = oss.str();

    { std::ofstream f(base + ".in", std::ios::binary); f << input; }
    { std::ofstream f(base + ".ref.out", std::ios::binary); f << bestOut; }
    { std::ofstream f(base + ".cur.out", std::ios::binary); f << curOut; }
}

// Print error with mutex to avoid interleaving
static void printErr(const std::string& msg) {
    std::lock_guard<std::mutex> lk(g_errMtx);
    std::cerr << msg << std::endl;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Disable stdout buffering so progress is visible in redirected logs
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    int targetTotal = (argc > 1) ? atoi(argv[1]) : 10000;
    int threadCount = (argc > 2) ? atoi(argv[2]) : 16;
    int quickMode  = (argc > 3) ? atoi(argv[3]) : 0;

    _mkdir(FAIL_DIR);

    // Calculate actual total
    int totalCases = 0;
    for (int i = 0; i < kConfigCount; ++i) {
        int n = quickMode ? (std::min)(quickMode, kConfigs[i].seedCount) : kConfigs[i].seedCount;
        totalCases += n;
    }
    std::cout << "=== Big Integer Differential Tester ===" << std::endl;
    std::cout << "Target: " << targetTotal << ", Actual: " << totalCases
              << ", Threads: " << threadCount << std::endl;
    std::cout << "Generators: " << kConfigCount << std::endl;
    std::cout << "Reference: official correct.cpp (ref_*.exe)" << std::endl;
    std::cout << "Current:  dev solutions (cur_*.exe)" << std::endl;
    std::cout << std::endl;

    std::atomic<int> passed(0), failed(0), errors(0), done(0);
    // Per-op stats
    std::atomic<int> addPass(0), mulPass(0), divPass(0);
    std::atomic<int> addFail(0), mulFail(0), divFail(0);
    std::atomic<int> addErr(0),  mulErr(0),  divErr(0);

    auto t0 = std::chrono::steady_clock::now();

    ThreadPool pool(threadCount);
    std::atomic<int> lastPct{-1};

    for (int ci = 0; ci < kConfigCount; ++ci) {
        const GenConfig& cfg = kConfigs[ci];
        std::string genExe = genExePath(cfg.genName);
        std::string refExe = exePath((std::string("ref_") + cfg.op).c_str());
        std::string curExe  = exePath((std::string("cur_") + cfg.op).c_str());

        int actualSeeds = quickMode ? (std::min)(quickMode, cfg.seedCount) : cfg.seedCount;
        for (int seed = 0; seed < actualSeeds; ++seed) {
            pool.enqueue([&, genExe, refExe, curExe, cfg, seed]() {
                std::string seedStr = std::to_string(seed);

                // 1. Run generator
                ProcessResult genRes = runProcess(genExe, seedStr, "", 10000);
                if (!genRes.ok || genRes.timedOut) {
                    errors++;
                    done++;
                    if (cfg.op == std::string("add")) addErr++;
                    else if (cfg.op == std::string("mul")) mulErr++;
                    else divErr++;
                    if (errors.load() <= 10) {
                        std::ostringstream oss;
                        oss << "[ERR] gen " << cfg.genName << " seed=" << seed
                            << " ok=" << genRes.ok << " timeout=" << genRes.timedOut
                            << " exit=" << genRes.exitCode << " ms=" << genRes.ms
                            << " err='" << genRes.err.substr(0, 200) << "'";
                        printErr(oss.str());
                    }
                    return;
                }

                // 2. Run reference (official correct.cpp)
                ProcessResult refRes = runProcess(refExe, "", genRes.out, cfg.timeoutMs);
                if (!refRes.ok || refRes.timedOut) {
                    errors++;
                    done++;
                    if (cfg.op == std::string("add")) addErr++;
                    else if (cfg.op == std::string("mul")) mulErr++;
                    else divErr++;
                    if (errors.load() <= 10) {
                        std::ostringstream oss;
                        oss << "[ERR] ref " << cfg.genName << " seed=" << seed
                            << " ok=" << refRes.ok << " timeout=" << refRes.timedOut
                            << " exit=" << refRes.exitCode << " ms=" << refRes.ms
                            << " inSize=" << genRes.out.size()
                            << " err='" << refRes.err.substr(0, 200) << "'";
                        printErr(oss.str());
                    }
                    return;
                }

                // 3. Run current (dev solution)
                ProcessResult curRes = runProcess(curExe, "", genRes.out, cfg.timeoutMs);
                if (!curRes.ok || curRes.timedOut) {
                    errors++;
                    done++;
                    if (cfg.op == std::string("add")) addErr++;
                    else if (cfg.op == std::string("mul")) mulErr++;
                    else divErr++;
                    if (errors.load() <= 10) {
                        std::ostringstream oss;
                        oss << "[ERR] cur " << cfg.genName << " seed=" << seed
                            << " ok=" << curRes.ok << " timeout=" << curRes.timedOut
                            << " exit=" << curRes.exitCode << " ms=" << curRes.ms
                            << " inSize=" << genRes.out.size()
                            << " err='" << curRes.err.substr(0, 200) << "'";
                        printErr(oss.str());
                    }
                    return;
                }

                // 4. Compare (normalize line endings first)
                std::string refOut = normalizeLineEndings(refRes.out);
                std::string curOut = normalizeLineEndings(curRes.out);

                if (refOut == curOut) {
                    passed++;
                    if (cfg.op == std::string("add")) addPass++;
                    else if (cfg.op == std::string("mul")) mulPass++;
                    else divPass++;
                } else {
                    failed++;
                    if (cfg.op == std::string("add")) addFail++;
                    else if (cfg.op == std::string("mul")) mulFail++;
                    else divFail++;
                    saveFailure(cfg.op, cfg.genName, seed, genRes.out, refOut, curOut);
                    // Print first 5 failures
                    if (failed.load() <= 5) {
                        std::ostringstream oss;
                        oss << "[FAIL] " << cfg.genName << " seed=" << seed
                            << " refMs=" << refRes.ms << " curMs=" << curRes.ms
                            << " inSize=" << genRes.out.size();
                        printErr(oss.str());
                    }
                }

                int d = ++done;
                int pct = d * 100 / totalCases;
                if (pct % 5 == 0) {
                    int expected = pct;
                    if (lastPct.compare_exchange_strong(expected, pct)) {
                        auto t1 = std::chrono::steady_clock::now();
                        double sec = std::chrono::duration<double>(t1 - t0).count();
                        int p = passed.load(), f = failed.load(), e = errors.load();
                        std::cout << "[" << std::setw(3) << pct << "%] "
                                  << d << "/" << totalCases
                                  << " | pass=" << p << " fail=" << f << " err=" << e
                                  << " | " << std::fixed << std::setprecision(1) << sec << "s"
                                  << std::endl;
                    }
                }
            });
        }
    }

    // Wait for all tasks
    while (done.load() < totalCases) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Total:   " << done.load() << std::endl;
    std::cout << "Passed:  " << passed.load() << std::endl;
    std::cout << "Failed:  " << failed.load() << std::endl;
    std::cout << "Errors:  " << errors.load() << std::endl;
    std::cout << "Time:    " << std::fixed << std::setprecision(1) << sec << "s" << std::endl;

    std::cout << "\n--- Per-operation breakdown ---" << std::endl;
    std::cout << "ADD: pass=" << addPass.load() << " fail=" << addFail.load() << " err=" << addErr.load() << std::endl;
    std::cout << "MUL: pass=" << mulPass.load() << " fail=" << mulFail.load() << " err=" << mulErr.load() << std::endl;
    std::cout << "DIV: pass=" << divPass.load() << " fail=" << divFail.load() << " err=" << divErr.load() << std::endl;

    if (failed.load() > 0) {
        std::cout << "\nFailed cases saved to: " << FAIL_DIR << "\\" << std::endl;
    }

    return (failed.load() == 0 && errors.load() == 0) ? 0 : 1;
}
