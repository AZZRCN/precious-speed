// vm_bench.cpp - Precise benchmark driver for VM (Linux)
// Uses fork+exec+clock_gettime(CLOCK_MONOTONIC) for ms-level precision.
// Single-threaded sequential execution to avoid interference.
// Usage: ./vm_bench [runs]
//   runs: number of timed runs per case (default 5)

#include <chrono>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <string>

extern char** environ;

struct TestCase {
    const char* exe;
    const char* input;
    const char* label;
};

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// Run exe with input file, return elapsed ms (or -1 on failure)
static double run_once(const char* exe, const char* input_file) {
    int fd_in = open(input_file, O_RDONLY);
    if (fd_in < 0) return -1;

    // Pipe for stdout (discarded)
    int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null < 0) { close(fd_in); return -1; }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fd_in, STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&fa, dev_null, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&fa, dev_null, STDERR_FILENO);
    posix_spawn_file_actions_addclose(&fa, fd_in);
    posix_spawn_file_actions_addclose(&fa, dev_null);

    char* const argv[] = { (char*)exe, nullptr };
    pid_t pid;
    double t0 = now_ms();
    int rc = posix_spawn(&pid, exe, &fa, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fd_in);
    close(dev_null);

    if (rc != 0) return -1;

    int status;
    waitpid(pid, &status, 0);
    double t1 = now_ms();

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return -2;
    return t1 - t0;
}

static double bench(const char* exe, const char* input_file, int runs) {
    // Warmup
    run_once(exe, input_file);
    // Timed runs
    double best = 1e9;
    bool ok = false;
    for (int i = 0; i < runs; i++) {
        double t = run_once(exe, input_file);
        if (t > 0) {
            ok = true;
            if (t < best) best = t;
        }
    }
    return ok ? best : -1;
}

int main(int argc, char** argv) {
    int runs = (argc > 1) ? atoi(argv[1]) : 5;

    std::vector<TestCase> cases = {
        // ADD
        {"best_add", "add_max_0.in", "best_add max"},
        {"cur_add",  "add_max_0.in", "cur_add  max"},
        // MUL
        {"best_mul", "mul_max_0.in", "best_mul max"},
        {"cur_mul",  "mul_max_0.in", "cur_mul  max"},
        // DIV
        {"best_div", "div_max_0.in",  "best_div max_0 (A=B)"},
        {"cur_div",  "div_max_0.in",  "cur_div  max_0 (A=B)"},
        {"best_div", "div_max_2.in",  "best_div max_2 (A<B)"},
        {"cur_div",  "div_max_2.in",  "cur_div  max_2 (A<B)"},
        {"best_div", "div_medium_0.in", "best_div medium"},
        {"cur_div",  "div_medium_0.in", "cur_div  medium"},
        {"best_div", "div_large_0.in",  "best_div large"},
        {"cur_div",  "div_large_0.in",  "cur_div  large"},
    };

    printf("============================================================\n");
    printf("VM Benchmark (fork+exec+clock_gettime, runs=%d)\n", runs);
    printf("============================================================\n");
    printf("CPU: %d cores\n", (int)sysconf(_SC_NPROCESSORS_ONLN));
    printf("\n");

    struct Result { const char* label; double ms; };
    std::vector<Result> results;

    for (const auto& c : cases) {
        // Check exe exists
        struct stat st;
        if (stat(c.exe, &st) != 0) {
            printf("  %-30s: SKIP (no exe)\n", c.label);
            results.push_back({c.label, -1});
            continue;
        }
        if (stat(c.input, &st) != 0) {
            printf("  %-30s: SKIP (no input)\n", c.label);
            results.push_back({c.label, -1});
            continue;
        }
        double t = bench(c.exe, c.input, runs);
        if (t < 0) {
            printf("  %-30s: FAIL\n", c.label);
        } else {
            printf("  %-30s: %8.2f ms\n", c.label, t);
        }
        fflush(stdout);
        results.push_back({c.label, t});
    }

    printf("\n============================================================\n");
    printf("Comparison (cur vs best)\n");
    printf("============================================================\n");
    printf("  %-25s %10s %10s %10s\n", "test", "best(ms)", "cur(ms)", "diff");
    printf("  -----------------------------------------------------\n");

    const char* ops[] = {"add", "mul", "div"};
    for (const char* op : ops) {
        char buf[128];
        // max_0 comparison
        snprintf(buf, sizeof(buf), "%s_max_0 (A=B)", op);
        const char* inputs[] = {"add_max_0.in", "mul_max_0.in", "div_max_0.in"};
        const char* inp = nullptr;
        if (strcmp(op, "add") == 0) inp = "add_max_0.in";
        else if (strcmp(op, "mul") == 0) inp = "mul_max_0.in";
        else inp = "div_max_0.in";

        // Find best and cur results
        double best_t = -1, cur_t = -1;
        char best_label[64], cur_label[64];
        snprintf(best_label, sizeof(best_label), "best_%s max", op);
        snprintf(cur_label, sizeof(cur_label), "cur_%s  max", op);
        for (const auto& r : results) {
            if (strcmp(r.label, best_label) == 0) best_t = r.ms;
            if (strcmp(r.label, cur_label) == 0) cur_t = r.ms;
        }
        if (best_t > 0 && cur_t > 0) {
            double diff = (cur_t / best_t - 1) * 100;
            printf("  %-25s %10.2f %10.2f %+9.1f%%\n", buf, best_t, cur_t, diff);
        }
    }

    // DIV additional cases
    {
        const char* div_cases[] = {"max_2 (A<B)", "medium", "large"};
        const char* div_inputs[] = {"div_max_2.in", "div_medium_0.in", "div_large_0.in"};
        for (int i = 0; i < 3; i++) {
            double best_t = -1, cur_t = -1;
            char best_label[64], cur_label[64];
            snprintf(best_label, sizeof(best_label), "best_div %s", div_cases[i]);
            snprintf(cur_label, sizeof(cur_label), "cur_div  %s", div_cases[i]);
            for (const auto& r : results) {
                if (strcmp(r.label, best_label) == 0) best_t = r.ms;
                if (strcmp(r.label, cur_label) == 0) cur_t = r.ms;
            }
            if (best_t > 0 && cur_t > 0) {
                double diff = (cur_t / best_t - 1) * 100;
                printf("  %-25s %10.2f %10.2f %+9.1f%%\n", div_cases[i], best_t, cur_t, diff);
            } else {
                printf("  %-25s %10s %10s %10s\n", div_cases[i],
                       best_t > 0 ? "ok" : "FAIL",
                       cur_t > 0 ? "ok" : "FAIL", "-");
            }
        }
    }

    return 0;
}
