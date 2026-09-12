import subprocess, os

GEN_DIR = '/home/azzr/lcgen/division_of_big_integers/gen'
COMMON = '/home/azzr/lcgen/common'
D3DBG = '/home/azzr/divbench/bin/div_D3_dbg'

# compile debug D3 from current div_D3.cpp (already patched with D3 changes)
r = subprocess.run(['g++', '-O2', '-std=c++23', '-march=x86-64-v3',
                   '-I', COMMON, '/home/azzr/divbench/src/div_D3.cpp', '-o', D3DBG],
                  capture_output=True, text=True)
if r.returncode != 0:
    print('COMPILE_FAIL', r.stderr[:600]); raise SystemExit(1)

gens = {'a_max_b_random': 24, 'r_nearly_zero': 80, 'length_ratio_integer': 48}
max_eb = 0
cyclic_count = 0
total_eb = 0
for g, n in gens.items():
    gb = f'/tmp/gen_{g}'
    for seed in range(1, n + 1):
        inf = f'/tmp/stress_{g}_{seed}.in'
        p = subprocess.run([D3DBG, ], stdin=open(inf, 'rb'), capture_output=True)
        for line in p.stderr.decode(errors='replace').splitlines():
            if line.startswith('EB='):
                eb = int(line.split()[0].split('=')[1])
                cyc = line.split('CYCLIC=')[1] == '1'
                max_eb = max(max_eb, eb)
                total_eb += 1
                if cyc: cyclic_count += 1
print(f'calls={total_eb} max_est_blocks={max_eb} cyclic_true={cyclic_count}')
