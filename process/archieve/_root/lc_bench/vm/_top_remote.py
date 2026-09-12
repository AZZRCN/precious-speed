#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""函数级 Ir / D1mr / DLmr 排行 (Zen3 cache 参数)。"""
import subprocess, sys, re

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
B = sys.argv[1] if len(sys.argv) > 1 else 'd25'
C = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_00'
N = int(sys.argv[3]) if len(sys.argv) > 3 else 22

outf = '/tmp/cgT_%s_%s.out' % (B, C)
cache = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'
cmd = ('valgrind --tool=callgrind ' + cache + ' --callgrind-out-file=' + outf +
       ' ./bin/' + B + ' < ' + IN + C + '.in > /dev/null 2>/dev/null')
subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench')

ann = subprocess.run(['callgrind_annotate', '--threshold=99.5', outf],
                     capture_output=True, text=True).stdout

NUM = r'(?:[\d,]+(?:\s*\([\s\d.]+%\))?|\.)'


def val(t):
    t = t.strip()
    if t == '.':
        return 0
    t = t.split('(')[0]
    return int(t.replace(',', '').strip())


rx = re.compile(r'^\s*(%s)\s+(%s)\s+(%s)\s+(%s)\s+(%s)\s+(%s)\s+(%s)\s+(%s)\s+(%s)\s+(\S.*)$'
                % ((NUM,) * 9))
rows = []
tot = None
for line in ann.splitlines():
    if 'PROGRAM TOTALS' in line:
        nums = re.findall(NUM, line.split('PROGRAM')[0])
        try:
            tot = [val(x) for x in nums if x.strip()]
        except Exception:
            pass
        continue
    m = rx.match(line)
    if not m:
        continue
    g = [val(x) for x in m.groups()[:9]]
    name = m.group(10)
    if ' ' in name and ':' in name:
        name = name.split(':', 1)[1]
    rows.append((g[0], g[1], g[3], g[5], g[7], name))  # Ir, I1mr, ILmr?, ...

if tot:
    print('TOTALS Ir=%d' % tot[0])
    Tir = tot[0]
else:
    Tir = sum(r[0] for r in rows) or 1
print('%-13s %6s %11s %11s  %s' % ('Ir', '%', 'D1mr', 'DLmr', 'fn'))
print('-' * 100)
rows.sort(key=lambda r: -r[0])
for r in rows[:N]:
    nm = r[5]
    if len(nm) > 62:
        nm = nm[:59] + '...'
    print('%13d %5.2f%% %11d %11d  %s' % (r[0], 100.0 * r[0] / Tir, r[3], r[4], nm))
print('DONE')
