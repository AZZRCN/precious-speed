#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""扫描 MUL_BASIC_THRESHOLD / SQR_BASIC_THRESHOLD 对 Ir 的影响 (纯 Ir, 无 cache-sim)。

用法: python3 _multhr_remote.py div_D36 64,96,128,192 case1,case2,...
"""
import os
import subprocess
import sys

ROOT = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC = sys.argv[1] if len(sys.argv) > 1 else 'div_D36'
THRS = [int(x) for x in (sys.argv[2] if len(sys.argv) > 2 else '64,96,128,192').split(',')]
CASES = (sys.argv[3] if len(sys.argv) > 3
         else 'burnikel_ziegler_bound_02,r_nearly_zero_01,length_ratio_integer_02').split(',')

os.chdir(ROOT)
os.makedirs('bin', exist_ok=True)
res = {}

for t in THRS:
    tag = 'sw%d' % t
    cmd = ('g++ -O2 -std=c++23 -march=x86-64-v3 '
           '-DMUL_BASIC_THRESHOLD=%d -DSQR_BASIC_THRESHOLD=%d -o bin/%s src/%s.cpp'
           % (t, t, tag, SRC))
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print('COMPILE FAIL t=%d' % t, flush=True)
        print(r.stderr[-2500:], flush=True)
        continue
    for c in CASES:
        of = '/tmp/sw_%d_%s.out' % (t, c)
        subprocess.run('valgrind --tool=callgrind --callgrind-out-file=%s '
                       './bin/%s < %s%s.in > /dev/null 2>/dev/null'
                       % (of, tag, IN, c), shell=True)
        ir = None
        try:
            for line in open(of):
                if line.startswith('summary:'):
                    ir = int(line.split()[1])
                    break
        except Exception as ex:
            print('  read fail %s' % ex, flush=True)
        res[(t, c)] = ir
        print('T=%-4d %-30s Ir=%s' % (t, c, ir), flush=True)

print('\n--- 相对 T=64 (=D34 行为) ---', flush=True)
for c in CASES:
    base = res.get((64, c))
    for t in THRS:
        v = res.get((t, c))
        if v and base:
            print('  %-30s T=%-4d Ir=%-12d %+.2f%%'
                  % (c, t, v, 100.0 * (float(v) / base - 1.0)), flush=True)
print('DONE', flush=True)
