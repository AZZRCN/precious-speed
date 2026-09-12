// tw_bench.cpp —— 两级 twiddle 表 (fft_ref) vs 单级大表 (fft_flat) 对照 + Ir/miss 实测
//   chk        : 逐字节对照 difRec / ditRec / difRecZeroHi / pointwise / pointwiseSq
//   kern_ref   : 只跑蝶形 DIF+DIT (两级表)
//   kern_flat  : 只跑蝶形 DIF+DIT (单级大表)
//   conv_ref   : DIF+pointwise+DIT (两级表)
//   conv_flat  : DIF+pointwise+DIT (单级大表)
// g++ -O3 -march=haswell -g -no-pie -std=c++17 tw_bench.cpp -o tw_bench
#include "prod_fft.h"
#include "prod_fft1.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

using cpx = __m128d;

static inline cpx mk(double r, double i) { return _mm_set_pd(i, r); }

static void fill_rand(std::vector<cpx>& v, uint64_t seed) {
    std::mt19937_64 rng(seed);
    for (auto& z : v) {
        double r = (double)(rng() & 0xffff) / 65536.0 - 0.5;
        double i = (double)(rng() & 0xffff) / 65536.0 - 0.5;
        z = mk(r, i);
    }
}

// 逐字节比较两块 cpx 数据
static bool bytecmp(const std::vector<cpx>& a, const std::vector<cpx>& b) {
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(cpx)) == 0;
}

static int firstdiff(const std::vector<cpx>& a, const std::vector<cpx>& b) {
    const double* pa = (const double*)a.data();
    const double* pb = (const double*)b.data();
    for (size_t k = 0; k != a.size() * 2; ++k)
        if (std::memcmp(pa + k, pb + k, 8) != 0) return (int)(k >> 1);
    return -1;
}

static int do_chk() {
    const u32 ns[] = {256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
    int fails = 0;
    for (u32 n : ns) {
        std::vector<cpx> in(n), A(n), B(n), inG(n), AG(n), BG(n);
        fill_rand(in, 1000 + n);
        fill_rand(inG, 7000 + n);

        // ---- difRec ----
        fft_ref::resize(n);  A = in;  fft_ref::difRec(A.data(), n, 0);
        fft_flat::resize(n); B = in;  fft_flat::difRec(B.data(), n, 0);
        bool ok = bytecmp(A, B);
        printf("%-14s n=%-7u %s", "difRec", n, ok ? "BYTE-EQ\n" : "");
        if (!ok) { printf("DIFF at k=%d\n", firstdiff(A, B)); ++fails; }

        // ---- ditRec ----
        fft_ref::resize(n);  A = in;  fft_ref::ditRec(A.data(), n, 0);
        fft_flat::resize(n); B = in;  fft_flat::ditRec(B.data(), n, 0);
        ok = bytecmp(A, B);
        printf("%-14s n=%-7u %s", "ditRec", n, ok ? "BYTE-EQ\n" : "");
        if (!ok) { printf("DIFF at k=%d\n", firstdiff(A, B)); ++fails; }

        // ---- difRecZeroHi ----
        {
            std::vector<cpx> z = in;
            for (u32 k = n >> 1; k != n; ++k) z[k] = _mm_setzero_pd();
            fft_ref::resize(n);  A = z; fft_ref::difRecZeroHi(A.data(), n);
            fft_flat::resize(n); B = z; fft_flat::difRecZeroHi(B.data(), n);
            ok = bytecmp(A, B);
            printf("%-14s n=%-7u %s", "difRecZeroHi", n, ok ? "BYTE-EQ\n" : "");
            if (!ok) { printf("DIFF at k=%d\n", firstdiff(A, B)); ++fails; }
        }

        // ---- pointwise ----
        fft_ref::resize(n);  A = in; AG = inG; fft_ref::pointwise(A.data(), AG.data(), n);
        fft_flat::resize(n); B = in; BG = inG; fft_flat::pointwise(B.data(), BG.data(), n);
        ok = bytecmp(A, B);
        printf("%-14s n=%-7u %s", "pointwise", n, ok ? "BYTE-EQ\n" : "");
        if (!ok) { printf("DIFF at k=%d\n", firstdiff(A, B)); ++fails; }

        // ---- pointwiseSq ----
        fft_ref::resize(n);  A = in; fft_ref::pointwiseSq(A.data(), n);
        fft_flat::resize(n); B = in; fft_flat::pointwiseSq(B.data(), n);
        ok = bytecmp(A, B);
        printf("%-14s n=%-7u %s", "pointwiseSq", n, ok ? "BYTE-EQ\n" : "");
        if (!ok) { printf("DIFF at k=%d\n", firstdiff(A, B)); ++fails; }
    }
    printf(fails ? "=== FAILURES PRESENT (%d) ===\n" : "=== ALL BYTE-EQUAL ===\n", fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "chk";
    if (mode == "chk") return do_chk();

    u32 TS = argc > 3 ? (u32)std::strtoul(argv[3], nullptr, 10) : 65536;
    u32 iters = argc > 2 ? (u32)std::strtoul(argv[2], nullptr, 10) : 200;

    std::vector<cpx> srcF(TS), srcG(TS);
    fill_rand(srcF, 424242);
    fill_rand(srcG, 191919);
    std::vector<cpx> F(TS), G(TS);
    double sink = 0;

    if (mode == "kern_ref" || mode == "conv_ref") {
        fft_ref::resize(TS);
        for (u32 it = 0; it != iters; ++it) {
            F = srcF; G = srcG;
            fft_ref::difRec(F.data(), TS, 0);
            fft_ref::difRec(G.data(), TS, 0);
            if (mode == "conv_ref") fft_ref::pointwise(F.data(), G.data(), TS);
            fft_ref::ditRec(F.data(), TS, 0);
        }
    } else if (mode == "kern_flat" || mode == "conv_flat") {
        fft_flat::resize(TS);
        for (u32 it = 0; it != iters; ++it) {
            F = srcF; G = srcG;
            fft_flat::difRec(F.data(), TS, 0);
            fft_flat::difRec(G.data(), TS, 0);
            if (mode == "conv_flat") fft_flat::pointwise(F.data(), G.data(), TS);
            fft_flat::ditRec(F.data(), TS, 0);
        }
    } else { printf("bad mode\n"); return 1; }

    for (u32 k = 0; k != TS; ++k) sink += _mm_cvtsd_f64(F[k]);
    printf("%s done iters=%u TS=%u (sink=%.6e)\n", mode.c_str(), iters, TS, sink);
    return 0;
}
