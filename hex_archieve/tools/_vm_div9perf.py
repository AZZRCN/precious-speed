#!/usr/bin/env python3
# 编排: 把 v8 baseline + v9 推到 VM, 编译, 生成大输入, 跑 _div_perf.py (min-of-10 ratio-of-mins)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
ROOT = 'D:/hex_precious_speed'
DB = '/home/azzr/divbench'; SRC = DB + '/src'; BIN = DB + '/bin'
GEN = '/home/azzr/lcgen/division_of_big_integers/gen'; COMMON = '/home/azzr/lcgen/common'
CXX = 'g++ -O2 -std=c++23 -march=x86-64-v3'
PD = DB + '/perfdiv'

vmctl.put(os.path.join(ROOT, 'submit_ready/div.cpp'), f'{SRC}/div_v8b.cpp')
vmctl.put(os.path.join(ROOT, 'work/div/v9.cpp'), f'{SRC}/div_v9.cpp')
vmctl.put(os.path.join(ROOT, 'tools/_div_perf.py'), f'{DB}/_div_perf.py')
print(vmctl.run(f'cd {DB} && {CXX} -o {BIN}/div_v8b {SRC}/div_v8b.cpp 2>&1 | tail -3; '
                f'{CXX} -o {BIN}/div_v9 {SRC}/div_v9.cpp 2>&1 | tail -3; mkdir -p {PD}')[1])

print(vmctl.run(' && '.join(
    f'g++ -O2 -std=c++17 -I {COMMON} {GEN}/{g}.cpp -o /tmp/gen_{g} 2>&1 | tail -1'
    for g in ['large', 'max', 'length_ratio_integer', 'a_max_b_random', 'burnikel_ziegler_bound']))[1])

shapes = [('max', 1), ('max', 2), ('length_ratio_integer', 1), ('length_ratio_integer', 2),
          ('large', 1), ('large', 2), ('a_max_b_random', 1), ('burnikel_ziegler_bound', 1)]
for g, seed in shapes:
    vmctl.run(f'/tmp/gen_{g} {seed} > {PD}/{g}_{seed}.in 2>/dev/null')
    print('case', f'{g}_{seed}.in', vmctl.run(f'wc -c {PD}/{g}_{seed}.in')[1].strip())

print(vmctl.run(f'cd {DB} && python3 _div_perf.py div_v8b div_v9 2>&1')[1])
