#!/usr/bin/env python3
"""perf 单点多二进制对比: perfone.py <datadir> <case1,case2,...> <bin> [bin ...]"""
import subprocess, sys, os

datadir = sys.argv[1]
cases = sys.argv[2].split(',')
bins = sys.argv[3:]


def perf_ir(binpath, inf, expf):
    with open(inf, 'rb') as f:
        data = f.read()
    r = subprocess.run(['perf', 'stat', '-x,', '-e', 'instructions:u,cycles:u',
                        binpath], input=data, capture_output=True)
    out = r.stdout
    exp = open(expf, 'rb').read() if os.path.exists(expf) else None
    ok = (exp is None) or (b' '.join(out.split()) == b' '.join(exp.split()))
    ir = cyc = 0
    for line in r.stderr.decode(errors='ignore').split('\n'):
        p = line.split(',')
        if len(p) > 2 and p[0].strip().isdigit():
            if 'instructions' in p[2]: ir = int(p[0])
            elif 'cycles' in p[2]: cyc = int(p[0])
    return ir, cyc, ok


hdr = '%-34s' % 'variant'
for c in cases:
    hdr += '%18s' % c[:17]
hdr += '%14s' % 'SUM'
print(hdr)
print('-' * len(hdr))
best = None
for b in bins:
    row = '%-34s' % os.path.basename(b)
    tot = 0
    for c in cases:
        inf = os.path.join(datadir, c + '.in')
        expf = os.path.join(datadir, c + '.exp')
        ir, cyc, ok = perf_ir(b, inf, expf)
        tot += ir
        row += '%17.1fM%s' % (ir / 1e6, '' if ok else '!')
    row += '%13.1fM' % (tot / 1e6)
    print(row, flush=True)
    if best is None or tot < best[1]:
        best = (os.path.basename(b), tot)
print('\nBEST: %s  sum=%.1fM' % (best[0], best[1] / 1e6))
