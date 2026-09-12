#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""两个二进制在同一 case 上的**函数级** Ir / D1mr / DLmr 差分。

  _fndiff_remote.py <binA> <binB> <case> [topn]

用于回答: 融合把成本挪到哪去了? 新增的 D1mr 落在哪个函数?
callgrind_annotate 的列形如 "12,345 (6.78%)" 或 "." (为 0), 需要专门解析。
"""
import subprocess, os, sys, re

REMOTE = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
CACHE = ('--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 '
         '--LL=33554432,16,64')

A = sys.argv[1]
B = sys.argv[2]
CASE = sys.argv[3]
TOPN = int(sys.argv[4]) if len(sys.argv) > 4 else 22

NUM = r'(?:[\d,]+(?:\s*\([\s\d.]+%\))?|\.)'
ROW = re.compile(r'^\s*(%s)\s+(%s)\s+(%s)\s+(\S.*)$' % (NUM, NUM, NUM))


def val(tok):
    tok = tok.strip()
    if tok == '.':
        return 0
    return int(tok.split('(')[0].strip().replace(',', ''))


def collect(b):
    outf = '/tmp/fnd_%s_%s.out' % (b, CASE)
    if not os.path.exists(outf):
        cmd = ('valgrind --tool=callgrind ' + CACHE +
               ' --callgrind-out-file=' + outf +
               ' ./bin/' + b + ' < ' + IN + CASE + '.in >/dev/null 2>/dev/null')
        if subprocess.run(cmd, shell=True, cwd=REMOTE).returncode != 0:
            sys.exit('run fail ' + b)
    r = subprocess.run(['callgrind_annotate', '--threshold=99.9',
                        '--show=Ir,D1mr,DLmr', outf],
                       capture_output=True, text=True)
    d = {}
    started = False
    total = None
    for ln in r.stdout.splitlines():
        if 'PROGRAM TOTALS' in ln:
            m = ROW.match(ln)
            if m:
                total = (val(m.group(1)), val(m.group(2)), val(m.group(3)))
            continue
        if 'file:function' in ln:
            started = True
            continue
        if not started:
            continue
        m = ROW.match(ln)
        if not m:
            continue
        fn = m.group(4).strip()
        fn = re.sub(r'\s*\[[^\]]*\]\s*$', '', fn).strip()
        if ':' in fn:
            fn = fn.split(':', 1)[1]
        fn = fn.replace('hint::transform::fft::', '').replace('hint::Integer::', '')
        fn = fn.replace('FFT<double>::', '').replace('(anonymous namespace)::', '')
        cur = d.get(fn, (0, 0, 0))
        d[fn] = (cur[0] + val(m.group(1)), cur[1] + val(m.group(2)),
                 cur[2] + val(m.group(3)))
    return d, total


da, ta = collect(A)
db, tb = collect(B)
keys = set(da) | set(db)


def est(t):
    return t[0] + 5 * t[1] + 200 * t[2]


print('### %s : %s vs %s' % (CASE, A, B))
print('%-24s %14s %14s' % ('TOTAL', A, B))
for i, nm in enumerate(['Ir', 'D1mr', 'DLmr']):
    print('%-24s %14d %14d  %+.2f%%' % (nm, ta[i], tb[i],
          (tb[i] / ta[i] - 1) * 100 if ta[i] else 0))
print('%-24s %14d %14d  %+.2f%%' % ('est', est(ta), est(tb),
      (est(tb) / est(ta) - 1) * 100))
print()
rows = []
for k in keys:
    a = da.get(k, (0, 0, 0))
    b = db.get(k, (0, 0, 0))
    rows.append((est(b) - est(a), k, a, b))
rows.sort(key=lambda r: -abs(r[0]))
print('%-44s %11s %11s | %9s %9s | %11s' %
      ('function', 'Ir_A', 'Ir_B', 'D1mr_A', 'D1mr_B', 'd_est'))
print('-' * 106)
for d, k, a, b in rows[:TOPN]:
    print('%-44s %11d %11d | %9d %9d | %+11d' % (k[:44], a[0], b[0], a[1], b[1], d))
