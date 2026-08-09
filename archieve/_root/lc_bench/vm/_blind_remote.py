#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 VM 内执行：找 est 模型的盲区。

用法: python3 _blind_remote.py <bin> <cases-csv> <reps>

1) wall-clock: 交替轮转跑 reps 轮, 丢首轮预热, 取 median
2) callgrind 全计数器 (--cache-sim=yes --branch-sim=yes, Zen3 参数)
   输出 Ir I1mr ILmr Dr D1mr DLmr Dw D1mw DLmw Bc Bcm Bi Bim
"""
import os, re, subprocess, statistics, sys, time

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BIN = sys.argv[1] if len(sys.argv) > 1 else 'd25'
CASES = (sys.argv[2] if len(sys.argv) > 2 else
         'length_ratio_integer_00,length_ratio_integer_01').split(',')
REPS = int(sys.argv[3] if len(sys.argv) > 3 else 7)

exe = '/home/azzr/divbench/bin/' + BIN

# ---------- 1. wall clock ----------
times = {c: [] for c in CASES}
for rep in range(REPS):
    order = CASES[rep % len(CASES):] + CASES[:rep % len(CASES)]
    for c in order:
        with open(IN + c + '.in', 'rb') as f:
            t0 = time.perf_counter()
            subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL)
            t1 = time.perf_counter()
        times[c].append((t1 - t0) * 1000.0)

print('== wall clock (ms, median of %d, warmup dropped) ==' % (REPS - 1))
wall = {}
for c in CASES:
    v = sorted(times[c][1:])
    wall[c] = statistics.median(v)
    print('%-30s med=%8.2f  min=%8.2f  max=%8.2f' % (c, wall[c], v[0], v[-1]))

# ---------- 2. callgrind full counters ----------
CG = ('valgrind --tool=callgrind --cache-sim=yes --branch-sim=yes '
      '--I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 '
      '--callgrind-out-file=/tmp/blind_%s_%s.out')

KEYS = ['Ir', 'I1mr', 'ILmr', 'Dr', 'D1mr', 'DLmr',
        'Dw', 'D1mw', 'DLmw', 'Bc', 'Bcm', 'Bi', 'Bim']

print('\n== callgrind full counters ==')
rows = {}
for c in CASES:
    out = '/tmp/blind_%s_%s.out' % (BIN, c)
    cmd = CG % (BIN, c) + ' ' + exe
    with open(IN + c + '.in', 'rb') as f:
        p = subprocess.run(cmd.split(), stdin=f,
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.PIPE)
    txt = p.stderr.decode('utf-8', 'replace')
    d = {}
    # 汇总行形如 "==pid== I   refs:      123,456,789"
    for line in txt.splitlines():
        m = re.search(r'\b(I|D)\s+refs:\s+([\d,]+)', line)
        if m:
            d['Ir' if m.group(1) == 'I' else 'Drw'] = int(m.group(2).replace(',', ''))
    # 用 callgrind_annotate 拿精确事件行
    ann = subprocess.run(['callgrind_annotate', '--threshold=0', out],
                         capture_output=True)
    at = ann.stdout.decode('utf-8', 'replace')
    m = re.search(r'^Events shown:\s*(.+)$', at, re.M)
    ev = m.group(1).split() if m else []
    m2 = re.search(r'^([\d,()\s.%]+)\s+PROGRAM TOTALS', at, re.M)
    if not m2:
        for line in at.splitlines():
            if 'PROGRAM TOTALS' in line:
                m2 = re.match(r'^([\d,()\s.%]+)', line)
                break
    vals = []
    if m2:
        vals = [int(x.replace(',', ''))
                for x in re.findall(r'([\d,]+)', m2.group(1))]
    d = dict(zip(ev, vals))
    rows[c] = d
    print('%-30s %s' % (c, ' '.join('%s=%s' % (k, d.get(k, '?')) for k in KEYS)))

# ---------- 3. 模型对比 ----------
print('\n== 模型 vs 实测 (以第一个 case 为基准的比值) ==')
base = CASES[0]


def est_plain(d):
    return d.get('Ir', 0) + 5 * d.get('D1mr', 0) + 200 * d.get('DLmr', 0)


def est_full(d):
    return (d.get('Ir', 0)
            + 5 * d.get('D1mr', 0) + 200 * d.get('DLmr', 0)
            + 5 * d.get('I1mr', 0) + 200 * d.get('ILmr', 0)
            + 1 * d.get('D1mw', 0) + 60 * d.get('DLmw', 0)
            + 15 * d.get('Bcm', 0) + 15 * d.get('Bim', 0))


print('%-30s %10s %10s %10s %8s %8s %8s'
      % ('case', 'wall', 'est', 'estFull', 'w/ratio', 'e/ratio', 'ef/rat'))
for c in CASES:
    d = rows[c]
    e, ef = est_plain(d) / 1e6, est_full(d) / 1e6
    print('%-30s %10.2f %10.1f %10.1f %8.3f %8.3f %8.3f'
          % (c, wall[c], e, ef,
             wall[c] / wall[base],
             e / (est_plain(rows[base]) / 1e6),
             ef / (est_full(rows[base]) / 1e6)))
