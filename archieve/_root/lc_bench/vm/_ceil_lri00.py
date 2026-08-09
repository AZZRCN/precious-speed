# -*- coding: utf-8 -*-
"""对 div_D39 跑 CEILHIST, 看 LC headline 用例 lri_00/lri_01 的 FFT 档位浪费。
用法: python _ceil_lri00.py [cases...]
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put as push

SRC = r'D:\precious_speed\best\div_D39.cpp'
REMOTE = '/home/azzr/divbench/div_D39.cpp'
CASES = sys.argv[1:] or ['length_ratio_integer_00', 'length_ratio_integer_01',
                         'length_ratio_integer_02']

print('[1] push')
print(push(SRC, REMOTE))

cmd = ('cd /home/azzr/divbench && g++ -O2 -std=c++23 -march=x86-64-v3 -DCEILHIST '
       '-o bin/d39ch div_D39.cpp 2>&1 | tail -5; echo BUILD_RC=$?')
rc, o, e = run(cmd, timeout=1200, verbose=False)
print('[2] compile:', o.strip(), e[-300:])

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in'
for c in CASES:
    cmd = ('cd /home/azzr/divbench && ./bin/d39ch < %s/%s.in > /dev/null' % (IN, c))
    rc, o, e = run(cmd, timeout=900, verbose=False)
    print('\n==== %s ====' % c)
    print(e.strip()[-3000:])
print('DONE')
