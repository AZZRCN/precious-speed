# -*- coding: utf-8 -*-
"""D41 vs D42: callgrind Ir/est 对比 (Zen3 cache 参数)。

est = Ir + 5*(D1mr+D1mw) + 200*(DLmr+DLmw)   —— 与既有口径一致。
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
RD = "/home/azzr/divbench"
BINS = sys.argv[1:] if len(sys.argv) > 1 else ["D41", "D42"]

REMOTE = r'''#!/usr/bin/env python3
import subprocess, os, sys
from concurrent.futures import ThreadPoolExecutor
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = __BINS__
CASES = __CASES__
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
'''

CASES = ['length_ratio_integer_00', 'length_ratio_integer_02',
         'r_nearly_zero_01', 'a_max_b_random_02',
         'burnikel_ziegler_bound_02', 'medium_01']

lo = [b.lower() for b in BINS]
for tag, b in zip(BINS, lo):
    lp = os.path.join(PS, "best", "div_%s.cpp" % tag)
    assert os.path.exists(lp), lp
    put(lp, "%s/div_%s.cpp" % (RD, tag))

run("mkdir -p %s/bin" % RD)
for tag, b in zip(BINS, lo):
    rc, o, e = run("cd %s && g++ -O2 -std=c++23 -march=x86-64-v3 div_%s.cpp -o bin/%s 2>&1 | tail -3"
                   % (RD, tag, b), timeout=1200)
    print("[build %s] rc=%d %s" % (b, rc, o.strip()[:200]), flush=True)

body = REMOTE.replace("__BINS__", repr(lo)).replace("__CASES__", repr(CASES))
tmp = os.path.join(HERE, "_d42_cg_remote.py")
with open(tmp, "w", encoding="utf-8", newline="\n") as f:
    f.write(body)
put(tmp, "%s/_d42_cg_remote.py" % RD)

rc, o, e = run("cd %s && python3 _d42_cg_remote.py" % RD, timeout=9000)
print(o)
if e.strip():
    print("ERR:", e.strip()[:500])
