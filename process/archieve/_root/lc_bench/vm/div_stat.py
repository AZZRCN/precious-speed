#!/usr/bin/env python3
"""DIV 结构画像: 给 div_D4 加确定性计数器(非计时), 跑 26 个官方点。

只统计与硬件无关的结构量, 用于定位瓶颈点的算法级浪费:
  - 分发路径计数 (basicCore / newtonCore1 / mu)
  - absDivMu 的块数、FFT 点数 (sum n*log2 n)、cyclic vs 线性
  - basicMul 回退次数与标量工作量
  - 修正循环迭代次数
"""
import re
import subprocess
import sys
from pathlib import Path

SRC_IN = Path('/home/azzr/divbench/src/div_D4.cpp')
SRC_OUT = Path('/home/azzr/divbench/src/div_stat.cpp')
BIN = Path('/home/azzr/divbench/bin/div_stat')
INDIR = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
FLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DDIVSTAT'

HEADER = r'''
// ==================== DIVSTAT (结构计数器, 非计时) ====================
#ifdef DIVSTAT
#include <cmath>
#include <vector>
#include <cstdio>
struct DsRec { size_t len1, len2, in, blocks; int cyc; double work; };
struct DivStat {
    size_t cb = 0, cc1 = 0, cmu = 0;
    double wba = 0, wc1 = 0, wfft = 0, wfb = 0;
    size_t nblk = 0, ncyc = 0, nlin = 0, nfb = 0, ncorr = 0;
    std::vector<DsRec> recs, basics, core1s;
    ~DivStat() { dump(); }
    void dump() {
        std::fprintf(stderr, "@@DS cb=%zu cc1=%zu cmu=%zu\n", cb, cc1, cmu);
        std::fprintf(stderr, "@@DS wba=%.6g wc1=%.6g wfft=%.6g wfb=%.6g\n", wba, wc1, wfft, wfb);
        std::fprintf(stderr, "@@DS nblk=%zu ncyc=%zu nlin=%zu nfb=%zu ncorr=%zu\n",
                     nblk, ncyc, nlin, nfb, ncorr);
        std::vector<DsRec> v = recs;
        std::sort(v.begin(), v.end(), [](const DsRec& a, const DsRec& b) { return a.work > b.work; });
        for (size_t i = 0; i < v.size() && i < 6; i++)
            std::fprintf(stderr, "@@MU len1=%zu len2=%zu in=%zu blocks=%zu cyc=%d work=%.6g\n",
                         v[i].len1, v[i].len2, v[i].in, v[i].blocks, v[i].cyc, v[i].work);
        std::vector<DsRec> b = basics;
        std::sort(b.begin(), b.end(), [](const DsRec& a, const DsRec& c) { return a.work > c.work; });
        for (size_t i = 0; i < b.size() && i < 3; i++)
            std::fprintf(stderr, "@@BA len1=%zu len2=%zu work=%.6g\n", b[i].len1, b[i].len2, b[i].work);
        std::vector<DsRec> c = core1s;
        std::sort(c.begin(), c.end(), [](const DsRec& a, const DsRec& d) { return a.work > d.work; });
        for (size_t i = 0; i < c.size() && i < 3; i++)
            std::fprintf(stderr, "@@C1 len1=%zu len2=%zu work=%.6g\n", c[i].len1, c[i].len2, c[i].work);
    }
};
static DivStat g_ds;
#define DS(...) do { __VA_ARGS__; } while (0)
#define DSD(...) __VA_ARGS__
#else
#define DS(...) ((void)0)
#define DSD(...)
#endif
// =====================================================================
'''

PATCHES = [
    ('#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;',
     HEADER + '\n#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;'),

    ('''                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);''',
     '''                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    DS(g_ds.cb++; double w = double(len1 - len2) * double(len2);
                       g_ds.wba += w; g_ds.basics.push_back({len1, len2, 0, 0, 0, w}));
                    absDivBasicCore(dividend_span, divisor_span, quot_span);'''),

    ('''                else if (len1 < len2 * 2)
                {
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);''',
     '''                else if (len1 < len2 * 2)
                {
                    DS(g_ds.cc1++; double w = double(len1) * std::log2(double(len1) + 2.0);
                       g_ds.wc1 += w; g_ds.core1s.push_back({len1, len2, 0, 0, 0, w}));
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);'''),

    ('''                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);''',
     '''                    DS(g_ds.cmu++);
                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);'''),

    ('''            size_t len1 = dividend.size, len2 = divisor.size;
            assert(in <= len2);''',
     '''            size_t len1 = dividend.size, len2 = divisor.size;
            DSD(double _ds_w0 = g_ds.wfft; size_t _ds_b0 = g_ds.nblk;)
            assert(in <= len2);'''),

    ('''                fftMulPre(divid_high, inv_dft_buf.data(), inv_span.size, inv_float_len, qhat_full);''',
     '''                DS(g_ds.nblk++;
                   g_ds.wfft += double(inv_float_len) * std::log2(double(inv_float_len)));
                fftMulPre(divid_high, inv_dft_buf.data(), inv_span.size, inv_float_len, qhat_full);'''),

    ('''                    basicMul(divid_high, inv_span, qhat_full);''',
     '''                    DS(g_ds.nfb++;
                       g_ds.wfb += double(divid_high.size) * double(inv_span.size));
                    basicMul(divid_high, inv_span, qhat_full);'''),

    ('''                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);''',
     '''                    DS(g_ds.ncyc++;
                       g_ds.wfft += double(cyclic_m) * std::log2(double(cyclic_m)));
                    fftMulModBm1Pre(qhat_span, divisor_dft_mod_buf.data(), len2, cyclic_m, prod_mod_span);'''),

    ('''                    fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);''',
     '''                    DS(g_ds.nlin++;
                       g_ds.wfft += double(divisor_float_len) * std::log2(double(divisor_float_len)));
                    fftMulPre(qhat_span, divisor_dft_buf.data(), len2, divisor_float_len, prod_span);'''),

    ('''                    corr_cnt++;
                }''',
     '''                    corr_cnt++; DS(g_ds.ncorr++);
                }'''),

    ('''                    absSub1(qhat_span, 1, qhat_span);
                    corr_down++;''',
     '''                    absSub1(qhat_span, 1, qhat_span);
                    corr_down++; DS(g_ds.ncorr++);'''),

    ('''                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++;''',
     '''                    absAdd1(qhat_span, 1, qhat_span);
                    corr_up++; DS(g_ds.ncorr++);'''),

    ('''            // 清零高位（仅低 len2 位是有效 remainder）
            if (len2 < dividend.size) {''',
     '''            DS(g_ds.recs.push_back({len1, len2, in, g_ds.nblk - _ds_b0,
                                    int(use_cyclic), g_ds.wfft - _ds_w0}));
            // 清零高位（仅低 len2 位是有效 remainder）
            if (len2 < dividend.size) {'''),
]


def main():
    src = SRC_IN.read_text(encoding='utf-8', errors='replace')
    for i, (a, b) in enumerate(PATCHES):
        cnt = src.count(a)
        if cnt != 1:
            print(f'!! patch {i}: anchor count = {cnt} (需 1) -> abort')
            print('   head:', a.strip().splitlines()[0][:110])
            return 1
        src = src.replace(a, b, 1)
    SRC_OUT.write_text(src, encoding='utf-8')
    print(f'patched -> {SRC_OUT} ({len(src)} B)', flush=True)

    r = subprocess.run(f'g++ {FLAGS} -o {BIN} {SRC_OUT} 2>&1', shell=True,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('BUILD FAIL:\n' + r.stdout[-4000:])
        return 1
    print('build OK\n', flush=True)

    hdr = (f'{"case":26s} {"mu":>3s} {"c1":>3s} {"ba":>4s} {"blk":>6s} {"cyc":>6s} {"lin":>6s} '
           f'{"FFTwork":>10s} {"C1work":>10s} {"BAwork":>10s} {"fb":>4s} {"FBwork":>9s} {"corr":>5s}')
    print(hdr)
    print('-' * len(hdr))
    details = {}
    for p in sorted(INDIR.glob('*.in')):
        with open(p, 'rb') as f:
            r = subprocess.run([str(BIN)], stdin=f, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, timeout=300)
        err = r.stderr.decode('utf-8', 'replace')
        d = dict(re.findall(r'\b(cb|cc1|cmu|wba|wc1|wfft|wfb|nblk|ncyc|nlin|nfb|ncorr)=([\d.eE+-]+)', err))
        f_ = lambda k: float(d.get(k, 0) or 0)
        details[p.stem] = (
            re.findall(r'@@MU len1=(\d+) len2=(\d+) in=(\d+) blocks=(\d+) cyc=(\d+) work=([\d.eE+-]+)', err),
            re.findall(r'@@BA len1=(\d+) len2=(\d+) work=([\d.eE+-]+)', err),
            re.findall(r'@@C1 len1=(\d+) len2=(\d+) work=([\d.eE+-]+)', err),
        )
        print(f'{p.stem:26s} {d.get("cmu","0"):>3s} {d.get("cc1","0"):>3s} {d.get("cb","0"):>4s} '
              f'{d.get("nblk","0"):>6s} {d.get("ncyc","0"):>6s} {d.get("nlin","0"):>6s} '
              f'{f_("wfft"):>10.4g} {f_("wc1"):>10.4g} {f_("wba"):>10.4g} '
              f'{d.get("nfb","0"):>4s} {f_("wfb"):>9.4g} {d.get("ncorr","0"):>5s}', flush=True)

    print('\n=== TOP 调用形状 ===')
    for name, (mus, bas, c1s) in details.items():
        if not (mus or bas or c1s):
            continue
        print(f'-- {name}')
        for l1, l2, i, bl, cy, w in mus[:4]:
            print(f'   MU len1={int(l1):>8d} len2={int(l2):>8d} ratio={int(l1)/max(1,int(l2)):7.2f} '
                  f'in={int(i):>7d} blocks={int(bl):>5d} cyc={cy} work={float(w):.4g}')
        for l1, l2, w in bas[:2]:
            print(f'   BA len1={int(l1):>8d} len2={int(l2):>8d} work={float(w):.4g}')
        for l1, l2, w in c1s[:2]:
            print(f'   C1 len1={int(l1):>8d} len2={int(l2):>8d} ratio={int(l1)/max(1,int(l2)):7.2f} '
                  f'work={float(w):.4g}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
