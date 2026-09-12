# -*- coding: utf-8 -*-
"""D34 函数级 Ir 排行 (lri_02 / rnz_01 / bz_02), 结果写 _d34_top.txt."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run

OUT = r'D:\precious_speed\lc_bench\vm\_d34_top.txt'
CASES = ['length_ratio_integer_02', 'r_nearly_zero_01', 'burnikel_ziegler_bound_02']

buf = []
for c in CASES:
    rc, o, e = run('cd /home/azzr/divbench && python3 _top_remote.py d34 %s 24 2>&1'
                   % c, timeout=5400, verbose=False)
    buf.append('======== %s (rc=%s) ========' % (c, rc))
    buf.append(o or '')
    if e and e.strip():
        buf.append('ERR: ' + e[-500:])
    buf.append('')
    open(OUT, 'w', encoding='utf-8').write('\n'.join(buf))

print('written ' + OUT)
