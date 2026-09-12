#!/usr/bin/env python3
import subprocess, os, sys
from concurrent.futures import ThreadPoolExecutor
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = ['d42', 'd44', 'd45']
CASES = ['length_ratio_integer_00', 'length_ratio_integer_02', 'r_nearly_zero_01', 'a_max_b_random_02', 'burnikel_ziegler_bound_02', 'medium_01']
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'

def measure(a):
    b, c = a
    outf = '/tmp/cgd42_%s_%s.out' % (b, c)
    cmd = ('valgrind --tool=callgrind ' + CACHE + ' --callgrind-out-file=' + outf +
           ' ./bin/' + b + ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return (b, c, None)
    ev = tot = None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    os.remove(outf)
    return (b, c, dict(zip(ev, tot)) if ev and tot else None)

jobs = [(b, c) for c in CASES for b in BINS if os.path.exists(IN + c + '.in')]
res = {}
with ThreadPoolExecutor(max_workers=3) as ex:
    for b, c, d in ex.map(measure, jobs):
        res[(b, c)] = d
        print('  done %s/%s' % (b, c), flush=True)

def est(d):
    if not d: return None
    return (d.get('Ir',0) + 5*(d.get('D1mr',0)+d.get('D1mw',0))
            + 200*(d.get('DLmr',0)+d.get('DLmw',0)))

A = BINS[0]
print('')
hdr = '%-28s' % 'case'
for b in BINS:
    hdr += ' %12s %7s' % (b+' est', 'r')
print(hdr)
print('-'*(28+len(BINS)*21))
rows = []
for c in CASES:
    if any(not res.get((b,c)) for b in BINS):
        print('%-28s FAIL' % c); continue
    ea = est(res[(A,c)])/1e6
    line = '%-28s' % c
    for b in BINS:
        e = est(res[(b,c)])/1e6
        line += ' %12.2f %7.4f' % (e, e/ea)
    print(line)
    rows.append((ea, c))
print('-'*(28+len(BINS)*21))
print('Ir (M):')
for c in CASES:
    if any(not res.get((b,c)) for b in BINS):
        continue
    ia = res[(A,c)]['Ir']/1e6
    line = '%-28s' % c
    for b in BINS:
        v = res[(b,c)]['Ir']/1e6
        line += ' %12.2f %7.4f' % (v, v/ia)
    print(line)
if rows:
    ea, c = max(rows)
    print('')
    print('HEADLINE(est max) = %s' % c)
    for b in BINS:
        e = est(res[(b,c)])/1e6
        print('   %-6s est=%9.2fM  ratio=%.4f   Ir=%9.2fM  ratio=%.4f'
              % (b, e, e/ea, res[(b,c)]['Ir']/1e6,
                 res[(b,c)]['Ir']/res[(A,c)]['Ir']))
