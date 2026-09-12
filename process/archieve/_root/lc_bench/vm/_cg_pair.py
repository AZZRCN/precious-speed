#!/usr/bin/env python3
import subprocess, os, re, sys

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = ["d14", "d15"]
CASES = ['length_ratio_integer_00', 'length_ratio_integer_03',
         'length_ratio_integer_01', 'length_ratio_integer_02',
         'a_max_b_random_02', 'max_00']

def ir(binname, case):
    """跑 callgrind, 返回 total instruction count"""
    outf = f'/tmp/cg_{binname}_{case}.out'
    cmd = (f'valgrind --tool=callgrind --cache-sim=no --branch-sim=no '
           f'--callgrind-out-file={outf} ./bin/{binname} '
           f'< {IN}{case}.in > /dev/null 2>/tmp/cg_{binname}_{case}.err')
    r = subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench')
    if r.returncode != 0:
        return None
    # callgrind 输出末尾 "summary:" 或 "totals:" 行
    tot = None
    with open(outf) as f:
        for line in f:
            if line.startswith('summary:') or line.startswith('totals:'):
                tot = int(line.split()[1])
    return tot

print('%-30s %14s %14s %8s' % ('case', 'D13 Ir', 'D14 Ir', 'ratio'), flush=True)
print('-' * 70, flush=True)
res = {}
for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        print(f'{c}: MISSING'); continue
    row = {}
    for b in BINS:
        row[b] = ir(b, c)
    if row['d13'] and row['d14']:
        r = row['d14'] / row['d13']
        res[c] = r
        print('%-30s %14d %14d %8.4f' % (c, row['d13'], row['d14'], r), flush=True)
    else:
        print(f'{c}: FAIL {row}', flush=True)

print('-' * 70, flush=True)
if res:
    worst = max(res.items(), key=lambda kv: kv[1])
    best = min(res.items(), key=lambda kv: kv[1])
    print(f'best  gain: {best[0]} ratio={best[1]:.4f}', flush=True)
    print(f'worst gain: {worst[0]} ratio={worst[1]:.4f}', flush=True)
print('DONE', flush=True)
