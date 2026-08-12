#!/usr/bin/env python3
# 在 VM 上运行: 对每个 case 跑 10 次 perf stat, 取 min (前辈纪律: >=10 次取 min, ratio-of-mins)
import subprocess, sys, re
BIN = '/home/azzr/divbench/bin'
PD = '/home/azzr/divbench/perfdiv'
bins = sys.argv[1:]
cases = ['max_1.in', 'max_2.in', 'length_ratio_integer_1.in', 'length_ratio_integer_2.in',
         'large_1.in', 'large_2.in', 'a_max_b_random_1.in', 'burnikel_ziegler_bound_1.in']


def run_once(b, case):
    p = subprocess.run(
        f'perf stat -e instructions:u,cycles:u,L1-dcache-load-misses,LLC-load-misses -r 1 {BIN}/{b} < {PD}/{case}',
        shell=True, capture_output=True, text=True)
    d = {}
    for m in re.finditer(r'([\d,]+)\s+([\w:-]+)', p.stderr):
        d[m.group(2)] = int(m.group(1).replace(',', ''))
    return d


for case in cases:
    print(f'--- {case} ---')
    res = {b: {} for b in bins}
    for b in bins:
        mins = {}
        for _ in range(10):
            d = run_once(b, case)
            for k, v in d.items():
                mins[k] = min(mins.get(k, v), v)
        res[b] = mins
    for k in ['instructions:u', 'cycles:u', 'L1-dcache-load-misses', 'LLC-load-misses']:
        vals = [res[b].get(k) for b in bins]
        line = f'  {k:24s} ' + '  '.join(f'{bins[i]}={vals[i]}' for i in range(len(bins)))
        if len(vals) == 2 and vals[0]:
            line += f'   ratio(v9/v8)={vals[1]/vals[0]:.4f}'
        print(line)
