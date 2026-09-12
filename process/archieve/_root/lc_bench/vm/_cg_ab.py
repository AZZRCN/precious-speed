#!/usr/bin/env python3
import subprocess, os, sys
from concurrent.futures import ThreadPoolExecutor
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = ['d31', 'd34', 'd39']
CASES = ['burnikel_ziegler_bound_00', 'burnikel_ziegler_bound_02', 'burnikel_ziegler_bound_03', 'a_max_b_random_01', 'a_max_b_random_02', 'r_nearly_zero_01', 'length_ratio_integer_00', 'length_ratio_integer_02', 'length_ratio_integer_05', 'medium_02', 'large_01', 'medium_01']
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'

def measure(args):
    b, c = args
    outf = '/tmp/cgab_%s_%s.out' % (b, c)
    cmd = ('valgrind --tool=callgrind ' + CACHE +
           ' --callgrind-out-file=' + outf + ' ./bin/' + b +
           ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return (b, c, None)
    ev, tot = None, None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    os.remove(outf)
    if not ev or not tot:
        return (b, c, None)
    return (b, c, dict(zip(ev, tot)))

jobs = [(b, c) for c in CASES for b in BINS if os.path.exists(IN + c + '.in')]
res = {}
with ThreadPoolExecutor(max_workers=3) as ex:
    for b, c, d in ex.map(measure, jobs):
        res[(b, c)] = d
        print('  done %s/%s' % (b, c), flush=True)

def est(d):
    if not d: return None
    Ir = d.get('Ir', 0)
    d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
    dl = d.get('DLmr', 0) + d.get('DLmw', 0)
    return Ir + 5 * d1 + 200 * dl, Ir, d1, dl

base = BINS[0]
print('', flush=True)
hdr = '%-28s' % 'case'
for b in BINS: hdr += ' %13s' % b
for b in BINS[1:]: hdr += ' %8s' % ('r:' + b)
print(hdr, flush=True); print('-' * len(hdr), flush=True)
ratios = {b: [] for b in BINS[1:]}
tot = {b: 0 for b in BINS}
for c in CASES:
    if (base, c) not in res: continue
    e0 = est(res[(base, c)])
    if not e0: continue
    line = '%-28s' % c
    for b in BINS:
        e = est(res.get((b, c)))
        line += ' %13d' % (e[0] if e else -1)
        if e: tot[b] += e[0]
    for b in BINS[1:]:
        e = est(res.get((b, c)))
        if e:
            r = e[0] / e0[0]; ratios[b].append(r)
            line += ' %8.4f' % r
        else:
            line += ' %8s' % '-'
    print(line, flush=True)
print('-' * len(hdr), flush=True)
line = '%-28s' % 'TOTAL'
for b in BINS: line += ' %13d' % tot[b]
for b in BINS[1:]: line += ' %8.4f' % (tot[b] / tot[base] if tot[base] else 0)
print(line, flush=True)
import statistics
for b in BINS[1:]:
    if ratios[b]:
        rs = sorted(ratios[b])
        print('  %s: median=%.4f  min=%.4f  max=%.4f  (n=%d)' %
              (b, statistics.median(rs), rs[0], rs[-1], len(rs)), flush=True)
# 每个 bin 的瓶颈点 (LC max 指标由它决定)
print('', flush=True)
for b in BINS:
    rows = [(c, est(res[(b, c)])[0]) for c in CASES if res.get((b, c))]
    rows.sort(key=lambda x: -x[1])
    print('  %s bottleneck: %s' % (b, ', '.join('%s=%d' % (c, v) for c, v in rows[:3])), flush=True)
print('DONE', flush=True)
