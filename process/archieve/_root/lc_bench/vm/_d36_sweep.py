# -*- coding: utf-8 -*-
"""上传 D36 + 扫描 MUL_BASIC_THRESHOLD, 结果写 _d36_sweep.txt."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '_d36_sweep.txt')

put(r'D:\precious_speed\best\div_D36.cpp', '/home/azzr/divbench/src/div_D36.cpp')
put(os.path.join(HERE, '_multhr_remote.py'), '/home/azzr/divbench/_multhr_remote.py')

THRS = '64,96,128,192'
CASES = 'burnikel_ziegler_bound_02,r_nearly_zero_01,length_ratio_integer_02'
rc, o, e = run('cd /home/azzr/divbench && python3 _multhr_remote.py div_D36 %s %s 2>&1'
               % (THRS, CASES), timeout=7200, verbose=False)
txt = 'rc=%s\n%s\n' % (rc, o or '')
if e and e.strip():
    txt += 'ERR:\n' + e[-2000:]
open(OUT, 'w', encoding='utf-8').write(txt)
print('written ' + OUT)
