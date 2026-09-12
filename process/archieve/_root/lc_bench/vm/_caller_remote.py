#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""找出某个函数的调用者及各自贡献的 inclusive cost (callgrind cfn/calls 解析)。

用法: _caller_remote.py <bin> <case> <substr>
"""
import subprocess, sys, re

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
B = sys.argv[1] if len(sys.argv) > 1 else 'd25'
C = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_00'
SUB = sys.argv[3] if len(sys.argv) > 3 else 'memset'

outf = '/tmp/cgC_%s_%s.out' % (B, C)
cache = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'
cmd = ('valgrind --tool=callgrind ' + cache + ' --callgrind-out-file=' + outf +
       ' ./bin/' + B + ' < ' + IN + C + '.in > /dev/null 2>/dev/null')
subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench')

names = {}
cur_fn = None
cur_cfn = None
pending_calls = 0
agg = {}   # caller -> [ncalls, incl_Ir]
rx_name = re.compile(r'^(c?fn)=\((\d+)\)(?:\s+(.*))?$')
with open(outf, errors='replace') as f:
    for line in f:
        line = line.rstrip('\n')
        m = rx_name.match(line)
        if m:
            kind, cid, nm = m.group(1), m.group(2), m.group(3)
            if nm:
                names[cid] = nm
            if kind == 'fn':
                cur_fn = names.get(cid, '?' + cid)
                cur_cfn = None
            else:
                cur_cfn = names.get(cid, '?' + cid)
            continue
        if line.startswith('calls='):
            pending_calls = int(line.split('=', 1)[1].split()[0])
            continue
        if pending_calls and cur_cfn and SUB in cur_cfn:
            parts = line.split()
            if len(parts) >= 2:
                try:
                    ir = int(parts[1])
                except ValueError:
                    ir = 0
                e = agg.setdefault(cur_fn, [0, 0])
                e[0] += pending_calls
                e[1] += ir
            pending_calls = 0
            continue
        pending_calls = 0

print('callers of *%s* in %s / %s' % (SUB, B, C))
print('%12s %14s   %s' % ('ncalls', 'incl_Ir', 'caller'))
print('-' * 100)
for k, v in sorted(agg.items(), key=lambda kv: -kv[1][1])[:20]:
    nm = k if len(k) <= 70 else k[:67] + '...'
    print('%12d %14d   %s' % (v[0], v[1], nm))
print('DONE')
