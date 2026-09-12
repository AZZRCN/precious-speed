#!/usr/bin/env python3
"""DIV 代价模型标定: 对多个 schoolbook 上限 (SB_LIMIT) 采集结构计数。

计数 (全部为确定性计数器, 非计时):
  wfft  = sum over 顶层 FFT 变换 of n*log2(n)
  nxf   = 顶层 FFT 变换次数
  wsb   = sum over absDivBasicCore of qn*len2   (schoolbook 肢体乘加次数)
  ncall = 顶层非平凡 absDivRem 次数
  nbig  = 走 FFT 路径 (core1/mu) 的次数

分发条件改为:
  len2<=64 || qn<=64 || qn*len2 <= SB_LIMIT   -> schoolbook
(SB_LIMIT=0 即退化为 D4 现状, 用作基准点做拟合)

输出 model.json 供本地拟合。
"""
import json
import re
import subprocess
import sys
from pathlib import Path

SRC_IN = Path('/home/azzr/divbench/src/div_D4.cpp')
SRCDIR = Path('/home/azzr/divbench/src')
BINDIR = Path('/home/azzr/divbench/bin')
INDIR = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
OUT = Path('/home/azzr/divbench/model.json')
BASEFLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DDIVSTAT'

LIMITS = [0, 5000, 10000, 20000, 50000, 100000, 300000]

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


def main():
    src = SRC_IN.read_text(encoding='utf-8', errors='replace')
    for i, (a, b) in enumerate(PATCHES):
        if src.count(a) != 1:
            print(f'!! patch {i}: anchor count = {src.count(a)}')
            print('   head:', a.strip().splitlines()[0][:110])
            return 1
        src = src.replace(a, b, 1)
    p = SRCDIR / 'div_model.cpp'
    p.write_text(src, encoding='utf-8')
    print(f'patched -> {p}', flush=True)

    cases = sorted(INDIR.glob('*.in'))
    res = {'sizes': {c.stem: c.stat().st_size for c in cases}, 'runs': {}}

    for lim in LIMITS:
        b = BINDIR / f'div_model_{lim}'
        r = subprocess.run(f'g++ {BASEFLAGS} -DDIV_SB_LIMIT={lim} -o {b} {p} 2>&1',
                           shell=True, capture_output=True, text=True)
        if r.returncode != 0:
            print(f'BUILD FAIL lim={lim}:\n' + r.stdout[-3000:])
            return 1
        row = {}
        for c in cases:
            with open(c, 'rb') as f:
                rr = subprocess.run([str(b)], stdin=f, stdout=subprocess.DEVNULL,
                                    stderr=subprocess.PIPE, timeout=600)
            m = re.search(r'@@M wfft=([\d.eE+-]+) nxf=(\d+) wsb=([\d.eE+-]+) ncall=(\d+) nbig=(\d+)',
                          rr.stderr.decode('utf-8', 'replace'))
            if not m:
                print(f'!! no counters for {c.stem} lim={lim}')
                return 1
            row[c.stem] = [float(m.group(1)), int(m.group(2)), float(m.group(3)),
                           int(m.group(4)), int(m.group(5))]
        res['runs'][str(lim)] = row
        tw = sum(v[0] for v in row.values())
        ts = sum(v[2] for v in row.values())
        tn = sum(v[1] for v in row.values())
        print(f'lim={lim:>9d}  wfft={tw:.4g}  nxf={tn:>7d}  wsb={ts:.4g}', flush=True)

    OUT.write_text(json.dumps(res))
    print('wrote', OUT)
    return 0


if __name__ == '__main__':
    sys.exit(main())
