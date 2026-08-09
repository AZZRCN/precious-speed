#!/usr/bin/env python3
"""D6 vs D4 FFT 工作量对比 (确定性计数, 非计时, 合规)。

用 div_model.py 的 DIVSTAT 补丁机制, 对 div_D4.cpp (natural in) 与 div_D6.cpp
(0.85*in 缩放) 各打补丁编译, 跑官方 26 例, 采集每例 wfft = Σ n*log2(n) (顶层 FFT)。
对比瓶颈点 length_ratio_integer_02 等是否因 in 缩小而降档。
lim=0 => dispatch 退化为原边长阈值 (与 D4 现状一致)。
"""
import re
import subprocess
import sys
from pathlib import Path

SRCDIR = Path('/home/azzr/divbench/src')
BINDIR = Path('/home/azzr/divbench/bin')
INDIR = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
BASEFLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DDIVSTAT'

HEADER = r'''
// ==================== DIVSTAT model ====================
#ifdef DIVSTAT
#include <cmath>
#include <cstdio>
struct DivStat {
    double wfft = 0, wsb = 0;
    size_t nxf = 0, ncall = 0, nbig = 0;
    ~DivStat() {
        std::fprintf(stderr, "@@M wfft=%.10g nxf=%zu wsb=%.10g ncall=%zu nbig=%zu\n",
                     wfft, nxf, wsb, ncall, nbig);
    }
};
static DivStat g_ds;
#define DS(...) do { __VA_ARGS__; } while (0)
#else
#define DS(...) ((void)0)
#endif
#ifndef DIV_SB_LIMIT
#define DIV_SB_LIMIT 0
#endif
// =======================================================
'''

PATCHES = [
    ('#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;',
     HEADER + '\n#ifdef PROFILE_DIV\nstatic FILE *_prof_fp = nullptr;'),

    ('''                void dif(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));''',
     '''                void dif(Float inout[], size_t float_len)
                {
                    DS(if constexpr (RIRI_IN) { g_ds.wfft += double(float_len) * std::log2(double(float_len)); g_ds.nxf++; });
                    HINT_ASSUME(is_2pow(float_len));'''),

    ('''                void idit(Float inout[], size_t float_len)
                {
                    HINT_ASSUME(is_2pow(float_len));''',
     '''                void idit(Float inout[], size_t float_len)
                {
                    DS(if constexpr (RIRI_OUT) { g_ds.wfft += double(float_len) * std::log2(double(float_len)); g_ds.nxf++; });
                    HINT_ASSUME(is_2pow(float_len));'''),

    ('''        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            if (dividend.size <= divisor.size)''',
     '''        static void absDivBasicCore(Span dividend, View divisor, Span quotient)
        {
            DS(if (dividend.size > divisor.size)
                   g_ds.wsb += double(dividend.size - divisor.size) * double(divisor.size));
            if (dividend.size <= divisor.size)'''),

    ('''                Span quot_span(quotient.data.data(), len1 - len2);
                if (len2 <= 64 || (len1 - len2) <= 64)
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);
                }''',
     '''                Span quot_span(quotient.data.data(), len1 - len2);
                DS(g_ds.ncall++);
                if (len2 <= 64 || (len1 - len2) <= 64
                    || double(len1 - len2) * double(len2) <= double(DIV_SB_LIMIT))
                {
                    absDivBasicCore(dividend_span, divisor_span, quot_span);
                }'''),

    ('''                else if (len1 < len2 * 2)
                {
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);''',
     '''                else if (len1 < len2 * 2)
                {
                    DS(g_ds.nbig++);
                    absDivNewtonCore1(dividend_span, divisor_span, quot_span);'''),

    ('''                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);''',
     '''                    DS(g_ds.nbig++);
                    absDivMu(dividend_span, divisor_span, quot_span, in_used, allow_cyclic);'''),
]


def patch(src):
    for i, (a, b) in enumerate(PATCHES):
        if src.count(a) != 1:
            print(f'!! patch {i}: anchor count = {src.count(a)}')
            print('   head:', a.strip().splitlines()[0][:110])
            return None
        src = src.replace(a, b, 1)
    return src


def build(srcname, binname):
    src = (SRCDIR / srcname).read_text(encoding='utf-8', errors='replace')
    ps = patch(src)
    if ps is None:
        return False
    (SRCDIR / ('__' + binname + '.cpp')).write_text(ps)
    r = subprocess.run(f'g++ {BASEFLAGS} -DDIV_SB_LIMIT=0 -o {BINDIR/binname} {SRCDIR/("__"+binname+".cpp")} 2>&1',
                       shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(f'BUILD FAIL {binname}:\n' + r.stdout[-2000:])
        return False
    print('built', binname, flush=True)
    return True


def run_wfft(binname):
    row = {}
    for c in sorted(INDIR.glob('*.in')):
        with open(c, 'rb') as f:
            rr = subprocess.run([str(BINDIR / binname)], stdin=f,
                                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=600)
        m = re.search(r'@@M wfft=([\d.eE+-]+) nxf=(\d+) wsb=([\d.eE+-]+) ncall=(\d+) nbig=(\d+)',
                      rr.stderr.decode('utf-8', 'replace'))
        if not m:
            print(f'!! no counters for {c.stem}')
            return None
        row[c.stem] = [float(m.group(1)), int(m.group(2)), float(m.group(3)),
                       int(m.group(4)), int(m.group(5))]
    return row


def main():
    if not build('div_D4.cpp', 'div_cmp_D4'):
        return 1
    if not build('div_D6.cpp', 'div_cmp_D6'):
        return 1
    w4 = run_wfft('div_cmp_D4')
    w6 = run_wfft('div_cmp_D6')
    if w4 is None or w6 is None:
        return 1
    print(f"\n{'case':28s}{'D4_wfft':>13s}{'D6_wfft':>13s}{'ratio':>8s}{'nbig4':>7s}{'nbig6':>7s}", flush=True)
    t4 = t6 = 0
    order = sorted(w4.keys())
    # 把瓶颈点排前面
    for k in sorted(w4, key=lambda x: -w4[x][0]):
        w4v = w4[k][0]; w6v = w6[k][0]
        r = (w6v / w4v) if w4v > 0 else 1.0
        print(f"{k:28s}{w4[k][0]:13.4g}{w6[k][0]:13.4g}{r:8.3f}{w4[k][4]:7d}{w6[k][4]:7d}", flush=True)
        t4 += w4[k][0]; t6 += w6[k][0]
    print(f"{'TOTAL':28s}{t4:13.4g}{t6:13.4g}{t6/t4:8.3f}")
    print(f"\nFFT workload reduction D6 vs D4: {(1 - t6/t4)*100:.2f}%")
    return 0


if __name__ == '__main__':
    sys.exit(main())
