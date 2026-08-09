#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D17 codelet 阈值扫描: Zen3 est + FFT 递归调用次数 (零 wall-clock)。"""
import subprocess, os, sys, re

IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = sys.argv[1].split(',') if len(sys.argv) > 1 else ['d16', 'd17_64']
CASES = sys.argv[2].split(',') if len(sys.argv) > 2 else ['length_ratio_integer_03']

# AMD Zen3 (LC judge): L1d 32KB/8way/64B, LL 32MB/16way/64B
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'


def parse_calls(path):
    """返回 {被调用函数名: 调用次数} —— 解析 callgrind 的 cfn=/calls= 对。"""
    names = {}          # id -> name
    calls = {}
    cur_cfn = None
    rx_name = re.compile(r'^(c?fn)=\((\d+)\)(?:\s+(.*))?$')
    with open(path, errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            m = rx_name.match(line)
            if m:
                kind, cid, nm = m.group(1), m.group(2), m.group(3)
                if nm:
                    names[cid] = nm
                if kind == 'cfn':
                    cur_cfn = names.get(cid, '?' + cid)
                continue
            if line.startswith('calls='):
                n = int(line.split('=', 1)[1].split()[0])
                if cur_cfn is not None:
                    calls[cur_cfn] = calls.get(cur_cfn, 0) + n
                continue
    return calls


def measure(b, c):
    outf = '/tmp/cg17_%s_%s.out' % (b, c)
    cmd = ('valgrind --tool=callgrind ' + CACHE +
           ' --callgrind-out-file=' + outf + ' ./bin/' + b +
           ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return None, None
    ev, tot = None, None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    if not ev or not tot:
        return None, None
    return dict(zip(ev, tot)), parse_calls(outf)


KEY = ('dif<false>', 'dif<true>', 'idit<false>', 'idit<true>',
       'difFixed', 'iditFixed', 'difDispatch', 'iditDispatch',
       'difSmall', 'iditSmall', 'expand')

base = {}
for c in CASES:
    print('#' * 78, flush=True)
    print('### case = %s' % c, flush=True)
    hdr = '%-10s %13s %11s %9s %14s %8s' % ('bin', 'Ir', 'D1m', 'DLm', 'est_cycles', 'vs base')
    print(hdr, flush=True); print('-' * len(hdr), flush=True)
    callmap = {}
    for b in BINS:
        d, calls = measure(b, c)
        if not d:
            print('%-10s FAIL' % b, flush=True); continue
        Ir = d.get('Ir', 0)
        d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
        dl = d.get('DLmr', 0) + d.get('DLmw', 0)
        est = Ir + 5 * d1 + 200 * dl
        if c not in base:
            base[c] = est
        print('%-10s %13d %11d %9d %14d %7.3f' %
              (b, Ir, d1, dl, est, est / base[c]), flush=True)
        callmap[b] = calls
    # FFT 递归调用次数对比
    print('--- FFT recursion call counts ---', flush=True)
    allfn = set()
    for b, cm in callmap.items():
        for fn, n in cm.items():
            if any(k in fn for k in KEY) and n > 1000:
                allfn.add(fn)
    for fn in sorted(allfn):
        short = fn.split('::')[-1][:52]
        row = '  %-54s' % short
        for b in BINS:
            row += ' %10s' % ('{:,}'.format(callmap.get(b, {}).get(fn, 0)))
        print(row, flush=True)
    print('  %-54s' % '(bin)' + ''.join(' %10s' % b for b in BINS), flush=True)
print('DONE', flush=True)
