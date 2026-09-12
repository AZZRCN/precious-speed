#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在 VM 内执行：LL(末级缓存)容量敏感度扫描 + 全计数器正确解析。

用法: python3 _llsweep_remote.py <bin> <cases-csv> <ll-list-MB>

假设检验: LC 判题机有效 L3 份额若 < 32MB, 则 working-set 大的用例(lri_00 21.3MiB)
会独家 thrash, 而 LL=32MB 的模型完全看不见。
"""
import os, re, subprocess, sys

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BIN = sys.argv[1] if len(sys.argv) > 1 else 'd25'
CASES = (sys.argv[2] if len(sys.argv) > 2 else
         'length_ratio_integer_00,length_ratio_integer_01').split(',')
LLS = [int(x) for x in (sys.argv[3] if len(sys.argv) > 3 else '32,16,8,4').split(',')]

exe = '/home/azzr/divbench/bin/' + BIN


def parse_out(path):
    """直接读 callgrind out 文件头的 events: 行 + summary/totals 行。"""
    ev, tot = None, None
    with open(path, 'r', errors='replace') as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split(':', 1)[1].split()
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split(':', 1)[1].split()]
                break
    if not ev or not tot:
        return {}
    return dict(zip(ev, tot))


KEYS = ['Ir', 'I1mr', 'ILmr', 'Dr', 'D1mr', 'DLmr', 'Dw', 'D1mw', 'DLmw']

print('%-30s %5s %12s %10s %10s %10s %10s %12s'
      % ('case', 'LL', 'Ir', 'D1mr', 'DLmr', 'I1mr', 'ILmr', 'est(M)'))
res = {}
for c in CASES:
    for ll in LLS:
        out = '/tmp/ll_%s_%s_%d.out' % (BIN, c, ll)
        cmd = ['valgrind', '--tool=callgrind', '--cache-sim=yes',
               '--I1=32768,8,64', '--D1=32768,8,64',
               '--LL=%d,16,64' % (ll * 1024 * 1024),
               '--callgrind-out-file=' + out, exe]
        with open(IN + c + '.in', 'rb') as f:
            subprocess.run(cmd, stdin=f, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
        d = parse_out(out)
        est = (d.get('Ir', 0) + 5 * d.get('D1mr', 0) + 200 * d.get('DLmr', 0)) / 1e6
        res[(c, ll)] = (d, est)
        print('%-30s %5d %12d %10d %10d %10d %10d %12.1f'
              % (c, ll, d.get('Ir', 0), d.get('D1mr', 0), d.get('DLmr', 0),
                 d.get('I1mr', 0), d.get('ILmr', 0), est))
        sys.stdout.flush()

print('\n== 相对 %s 的 est 比值(每个 LL 档内) ==' % CASES[0])
for ll in LLS:
    b = res.get((CASES[0], ll), ({}, 1))[1]
    line = 'LL=%3dMB: ' % ll
    for c in CASES:
        e = res.get((c, ll), ({}, 0))[1]
        line += '%s=%.3f  ' % (c.replace('length_ratio_integer_', 'lri')
                               .replace('r_nearly_zero_', 'rnz')
                               .replace('a_max_b_random_', 'amb'), e / b if b else 0)
    print(line)
