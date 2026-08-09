#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FFT_C4_MIN / FFT_FIXED_MAX 联合扫描 (纯编译宏, 不改源码)。
   用 callgrind + Zen3 模型出 est, 与基准变体比值。
   usage: _c4sweep_remote.py <src> <cases> <"c4min:fixedmax,...">
"""
import subprocess, os, sys, re

REMOTE = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC = sys.argv[1] if len(sys.argv) > 1 else 'div_D25'
CASES = (sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03').split(',')
VARIANTS = (sys.argv[3] if len(sys.argv) > 3 else '32:32,64:32,64:64').split(',')
os.chdir(REMOTE)
os.makedirs('bin', exist_ok=True)

FLAGS = '-O2 -std=c++23 -march=x86-64-v3'


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


built = []
for v in VARIANTS:
    c4, fx = v.split(':')
    tag = 'sw_%s_%s' % (c4, fx)
    r = sh('g++ %s -DFFT_C4_MIN=%s -DFFT_FIXED_MAX=%s -o bin/%s src/%s.cpp'
           % (FLAGS, c4, fx, tag, SRC))
    if r.returncode != 0:
        print('COMPILE FAIL', v)
        print(r.stderr[-2500:])
        continue
    built.append((v, tag))
print('built:', [b[0] for b in built], flush=True)

# 先做一次正确性快查 (26 官方例 vs 第一个变体)
ref = built[0][1]
for v, tag in built[1:]:
    bad = []
    for case in sorted(os.listdir(IN)):
        if not case.endswith('.in'):
            continue
        a = sh('./bin/%s < %s%s' % (ref, IN, case))
        b = sh('./bin/%s < %s%s' % (tag, IN, case))
        if a.stdout != b.stdout:
            bad.append(case)
    print('%-8s vs %-8s : %s' % (v, built[0][0], bad if bad else 'ALL MATCH'), flush=True)

pat = re.compile(r'^(\w+):\s+([\d,]+)', re.M)
for case in CASES:
    print('#' * 70)
    print('### case =', case)
    print('%-10s %14s %11s %9s %14s %8s' % ('variant', 'Ir', 'D1m', 'DLm', 'est', 'ratio'))
    base = None
    for v, tag in built:
        out = '/tmp/sw_%s_%s.out' % (tag, case)
        sh('valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=%s '
           '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
           './bin/%s < %s%s.in > /dev/null 2>/dev/null' % (out, tag, IN, case))
        r = sh('callgrind_annotate %s 2>/dev/null | grep "PROGRAM TOTALS"' % out)
        nums = [int(x.replace(',', '')) for x in re.findall(r'([\d,]+) \(100', r.stdout)]
        if len(nums) < 9:
            print('%-10s PARSE FAIL' % v)
            continue
        Ir, Dr, Dw, I1mr, D1mr, D1mw, ILmr, DLmr, DLmw = nums[:9]
        est = Ir + 5 * (D1mr + D1mw) + 200 * (DLmr + DLmw)
        if base is None:
            base = est
        print('%-10s %14d %11d %9d %14d %8.3f'
              % (v, Ir, D1mr + D1mw, DLmr + DLmw, est, est / base), flush=True)
print('DONE')
