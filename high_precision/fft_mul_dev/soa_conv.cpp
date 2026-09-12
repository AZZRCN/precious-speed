// 全卷积 Ir 沙盘: DIF + pointwise + DIT ; SoA vs AoS 比对正确性 + callgrind Ir
// 用法: soa_conv <aos|soa> [iters]
#include "prod_fft.h"
#include "soa_fft.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>
#include <cstring>

using cpx = __m128d;
static cpx mk(double r, double i) { return _mm_set_pd(i, r); }

static const u32 TS = 65536;   // 复数点数 (真实 monster case 量级)
static const u32 NITER = 200;

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s <aos|soa> [iters]\n", argv[0]); return 1; }
    std::string mode = argv[1];
    u32 iters = (argc >= 3) ? (u32)atoll(argv[2]) : NITER;

    std::mt19937_64 rng(20260820);
    // 输入: 模拟 real-packed 复数信号 (随机)
    std::vector<cpx> inA(TS), inB(TS);
    std::vector<double> FrA(TS), FiA(TS), FrB(TS), FiB(TS);
    for (u32 k = 0; k != TS; ++k) {
        double ra=(double)(rng()&0xffff)/65536-0.5, ia=(double)(rng()&0xffff)/65536-0.5;
        double rb=(double)(rng()&0xffff)/65536-0.5, ib=(double)(rng()&0xffff)/65536-0.5;
        inA[k]=mk(ra,ia); inB[k]=mk(rb,ib);
        FrA[k]=ra; FiA[k]=ia; FrB[k]=rb; FiB[k]=ib;
    }

    if (mode == "aos") {
        fft_ref::resize(TS);
        std::vector<cpx> D(TS), G(TS);
        for (u32 it = 0; it != iters; ++it) {
            std::copy(inA.begin(), inA.end(), D.begin());
            std::copy(inB.begin(), inB.end(), G.begin());
            fft_ref::difRec(D.data(), TS, 0);
            fft_ref::difRec(G.data(), TS, 0);
            fft_ref::pointwise(D.data(), G.data(), TS);
            fft_ref::ditRec(D.data(), TS, 0);
        }
        double mx = 0;
        for (u32 k = 0; k != TS; ++k) {
            double r=_mm_cvtsd_f64(D[k]), i=_mm_cvtsd_f64(_mm_unpackhi_pd(D[k],D[k]));
            mx=std::fmax(mx,std::fabs(r)+std::fabs(i));
        }
        printf("AoS done iters=%u  (sanity maxabs=%.3e)\n", iters, mx);
    } else if (mode == "soa") {
        fft_soa::resize(TS);
        std::vector<double> fA(TS), fAi(TS), gA(TS), gAi(TS);
        for (u32 it = 0; it != iters; ++it) {
            std::copy(FrA.begin(), FrA.end(), fA.begin());
            std::copy(FiA.begin(), FiA.end(), fAi.begin());
            std::copy(FrB.begin(), FrB.end(), gA.begin());
            std::copy(FiB.begin(), FiB.end(), gAi.begin());
            fft_soa::difRec(fA.data(), fAi.data(), TS, 0, 0);
            fft_soa::difRec(gA.data(), gAi.data(), TS, 0, 0);
            fft_soa::pointwise(fA.data(), fAi.data(), gA.data(), gAi.data(), TS);
            fft_soa::ditRec(fA.data(), fAi.data(), TS, 0, 0);
        }
        double mx = 0;
        for (u32 k = 0; k != TS; ++k) mx=std::fmax(mx,std::fabs(fA[k])+std::fabs(fAi[k]));
        printf("SoA done iters=%u  (sanity maxabs=%.3e)\n", iters, mx);
    } else if (mode == "kern_aos") {
        fft_ref::resize(TS);
        std::vector<cpx> D(TS);
        for (u32 it = 0; it != iters; ++it) {
            std::copy(inA.begin(), inA.end(), D.begin());
            fft_ref::difRec(D.data(), TS, 0);
            fft_ref::ditRec(D.data(), TS, 0);
        }
        printf("AoS kern(dif+dit) done iters=%u\n", iters);
    } else if (mode == "kern_soa") {
        fft_soa::resize(TS);
        std::vector<double> fA(TS), fAi(TS);
        for (u32 it = 0; it != iters; ++it) {
            std::copy(FrA.begin(), FrA.end(), fA.begin());
            std::copy(FiA.begin(), FiA.end(), fAi.begin());
            fft_soa::difRec(fA.data(), fAi.data(), TS, 0, 0);
            fft_soa::ditRec(fA.data(), fAi.data(), TS, 0, 0);
        }
        printf("SoA kern(dif+dit) done iters=%u\n", iters);
    } else { printf("bad mode\n"); return 1; }
    return 0;
}
