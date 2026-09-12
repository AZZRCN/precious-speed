#!/usr/bin/env python3
import subprocess, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC = '/home/azzr/divbench/src/d16.cpp'
F = '-O2 -std=c++23 -march=x86-64-v3'
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'
DELTAS = [0, 1, 2, 3, 4, 5]
CASES = ['length_ratio_integer_02', 'length_ratio_integer_03', 'length_ratio_integer_01', 'a_max_b_random_02']

def build(d):
    b = '/home/azzr/divbench/bin/nbs%d' % d
    cmd = 'g++ %s -DDIV_MU_NB_DELTA=%d -o %s %s' % (F, d, b, SRC)
    r = subprocess.run(cmd, shell=True, capture_output=True)
    return b if r.returncode == 0 else None

def measure(b, c):
    outf = '/tmp/nbs.out'
    cmd = ('valgrind --tool=callgrind ' + CACHE + ' --callgrind-out-file=' + outf +
           ' ' + b + ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True).returncode != 0:
        return None
    ev = tot = None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    if not ev or not tot:
        return None
    d = dict(zip(ev, tot))
    Ir = d.get('Ir', 0)
    d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
    dl = d.get('DLmr', 0) + d.get('DLmw', 0)
    return Ir, d1, dl, Ir + 5 * d1 + 200 * dl

bins = {}
for d in DELTAS:
    b = build(d)
    print('build delta=%d -> %s' % (d, 'ok' if b else 'FAIL'), flush=True)
    if b:
        bins[d] = b

for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        continue
    print('', flush=True)
    print('=== %s' % c, flush=True)
    print('%6s %13s %10s %13s %8s' % ('delta', 'Ir', 'DLm', 'est_cycles', 'rel'),
          flush=True)
    rows = []
    for d in DELTAS:
        if d not in bins:
            continue
        r = measure(bins[d], c)
        if not r:
            print('%6d FAIL' % d, flush=True); continue
        rows.append((d,) + r)
    if not rows:
        continue
    base = min(r[4] for r in rows)
    for d, Ir, d1, dl, est in rows:
        print('%6d %13d %10d %13d %8.4f' % (d, Ir, dl, est, est / base), flush=True)
    bd = min(rows, key=lambda r: r[4])[0]
    print('  --> best delta = %d (nb = nb_min+%d)' % (bd, bd), flush=True)
print('', flush=True)
print('DONE', flush=True)
