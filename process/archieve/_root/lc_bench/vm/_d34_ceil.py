# -*- coding: utf-8 -*-
"""D34 的 fft_ceil 档位浪费 + 加档离线模拟, 结果写 _d34_ceil.txt."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '_d34_ceil.txt')

put(os.path.join(HERE, '_ceilhist_remote.py'),
    '/home/azzr/divbench/_ceilhist_remote.py')

CASES = 'length_ratio_integer_02,burnikel_ziegler_bound_02,r_nearly_zero_01'
rc, o, e = run('cd /home/azzr/divbench && python3 _ceilhist_remote.py div_D34 %s 2>&1'
               % CASES, timeout=3600, verbose=False)
txt = 'rc=%s\n%s\n' % (rc, o or '')
if e and e.strip():
    txt += 'ERR:\n' + e[-2000:]
open(OUT, 'w', encoding='utf-8').write(txt)
print('written ' + OUT)
