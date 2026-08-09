#!/usr/bin/env python3
import subprocess, os, re
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BIN = 'd40a'
CASES = ['length_ratio_integer_00']

# AMD Zen3 (LC judge): L1d 32KB/8way/64B, LL 32MB/16way/64B
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'

def measure(c):
    outf = '/tmp/cgz_%s.out' % c
    cmd = ('valgrind --tool=callgrind ' + CACHE +
           ' --callgrind-out-file=' + outf + ' ./bin/' + BIN +
           ' < ' + IN + c + '.in > /dev/null 2>/tmp/cgz_%s.err' % c)
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return None
    # callgrind_annotate 汇总更稳: 直接读 summary/totals 行
    ev, tot = None, None
    with open(outf) as f:
        for line in f:
            if line.startswith('events:'):
                ev = line.split()[1:]
            elif line.startswith('summary:') or line.startswith('totals:'):
                tot = [int(x) for x in line.split()[1:]]
    if not ev or not tot:
        return None
    d = dict(zip(ev, tot))
    return d

hdr = '%-26s %13s %11s %10s %13s %7s' % (
    'case', 'Ir', 'D1mr+w', 'DLmr+w', 'est_cycles', 'CPI')
print(hdr, flush=True)
print('-' * len(hdr), flush=True)
rows = []
for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        continue
    d = measure(c)
    if not d:
        print('%-26s FAIL' % c, flush=True); continue
    Ir = d.get('Ir', 0)
    d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
    dl = d.get('DLmr', 0) + d.get('DLmw', 0)
    est = Ir + 5 * d1 + 200 * dl
    rows.append((c, Ir, d1, dl, est))
    print('%-26s %13d %11d %10d %13d %7.3f' % (c, Ir, d1, dl, est, est / Ir),
          flush=True)
print('-' * len(hdr), flush=True)
rows.sort(key=lambda r: -r[4])
print('=== ranked by est_cycles (LC bottleneck proxy) ===', flush=True)
for i, r in enumerate(rows[:8]):
    print('  %d. %-26s est=%d  (Ir=%d, DLm=%d)' % (i + 1, r[0], r[4], r[1], r[3]),
          flush=True)
print('DONE', flush=True)
