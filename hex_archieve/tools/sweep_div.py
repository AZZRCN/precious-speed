#!/usr/bin/env python3
"""div 参数扫描: 只跑重点大点, 比较不同 -D 宏组合的 Instructions."""
import os, sys, itertools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

WS = '/home/azzr/hexbench'
ROOT = 'D:/hex_precious_speed'
BASE = '-O2 -std=c++23 -DEVAL -DONLINE_JUDGE -march=native'
SRC = sys.argv[1] if len(sys.argv) > 1 else 'work/div/v3.cpp'
CASES = sys.argv[2] if len(sys.argv) > 2 else \
    'length_ratio_integer_04,length_ratio_integer_03,length_ratio_integer_00,a_max_b_random_01,burnikel_ziegler_bound_01'

COMBOS = []
for cut in [64, 128, 256, 512, 1024]:
    for mbf in [48, 192, 384, 768]:
        COMBOS.append((cut, mbf))

vmctl.put(f'{ROOT}/{SRC}', f'{WS}/div/sw.cpp')
vmctl.put(f'{ROOT}/tools/perfone.py', f'{WS}/perfone.py')

cmds = []
names = []
for cut, mbf in COMBOS:
    nm = f'sw_c{cut}_m{mbf}'
    names.append((nm, cut, mbf))
    cmds.append(f'g++ {BASE} -DBZ_CUTOFF={cut} -DMULBF_MAX={mbf} -o {WS}/div/{nm} {WS}/div/sw.cpp 2>&1 | tail -3')
print('=== compile %d variants ===' % len(COMBOS), flush=True)
vmctl.run(' ; '.join(cmds) + ' ; echo COMPILED', timeout=3600)

binlist = ' '.join(f'{WS}/div/{n[0]}' for n in names)
vmctl.run(f'python3 {WS}/perfone.py {WS}/data/div {CASES} {binlist}', timeout=7200)
