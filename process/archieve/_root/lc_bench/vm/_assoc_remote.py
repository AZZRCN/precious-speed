#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""L1D 相联度扫描: 分离 capacity/compulsory miss 与 conflict miss。

同一容量 (32KB) 同一行大小 (64B), 只改相联度:
  8-way  = 真实 Zen3
  32-way / 512-way(全相联) = 假想机
若 D1mr 随相联度上升而暴跌 -> 说明 miss 主要是 **冲突失效**,
即 2 的幂步长把多条访存流打到同一 set; 这类 miss 可以用
错位/填充 (padding) 之类的布局手段消掉, 属算法层而非机器特调。
若几乎不变 -> compulsory/capacity, 只能靠减少 pass 数。
"""
import subprocess, sys

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = sys.argv[1].split(',') if len(sys.argv) > 1 else ['d25']
CASES = sys.argv[2].split(',') if len(sys.argv) > 2 else ['length_ratio_integer_00']
ASSOCS = [int(x) for x in (sys.argv[3].split(',') if len(sys.argv) > 3 else ['8', '32', '512'])]


def measure(b, c, assoc):
    outf = '/tmp/cgA_%s_%s_%d.out' % (b, c, assoc)
    cache = ('--cache-sim=yes --D1=32768,%d,64 --I1=32768,8,64 '
             '--LL=33554432,16,64' % assoc)
    cmd = ('valgrind --tool=callgrind ' + cache +
           ' --callgrind-out-file=' + outf + ' ./bin/' + b +
           ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return None
    ev, tot = None, None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    if not ev or not tot:
        return None
    return dict(zip(ev, tot))


for c in CASES:
    print('#' * 78, flush=True)
    print('### case = %s' % c, flush=True)
    hdr = '%-8s %6s %13s %11s %11s %9s %14s' % (
        'bin', 'assoc', 'Ir', 'D1mr', 'D1mw', 'DLm', 'est_cycles')
    print(hdr, flush=True)
    print('-' * len(hdr), flush=True)
    for b in BINS:
        ref = None
        for a in ASSOCS:
            d = measure(b, c, a)
            if not d:
                print('%-8s %6d FAIL' % (b, a), flush=True)
                continue
            Ir = d.get('Ir', 0)
            d1r, d1w = d.get('D1mr', 0), d.get('D1mw', 0)
            dl = d.get('DLmr', 0) + d.get('DLmw', 0)
            est = Ir + 5 * (d1r + d1w) + 200 * dl
            if ref is None:
                ref = d1r
            print('%-8s %6d %13d %11d %11d %9d %14d   D1mr/8way=%.3f' %
                  (b, a, Ir, d1r, d1w, dl, est, d1r / ref), flush=True)
print('DONE', flush=True)
