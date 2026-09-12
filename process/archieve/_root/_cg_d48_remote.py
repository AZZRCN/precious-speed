#!/usr/bin/env python3
import subprocess, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
CASES = ['r_nearly_zero_01', 'r_nearly_zero_00', 'r_nearly_zero_02', 'length_ratio_integer_02', 'length_ratio_integer_04', 'a_max_b_random_00', 'large_00', 'medium_00', 'small_00', 'max_01']
BINS = ['d47', 'd48']
CACHE = ('--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 '
         '--LL=33554432,16,64')

def measure(b, c):
    outf = '/tmp/cgd48_%s_%s.out' % (b, c)
    cmd = ('valgrind --tool=callgrind ' + CACHE +
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
    os.unlink(outf)
    if not ev or not tot:
        return None
    return dict(zip(ev, tot))

hdr = '%-26s %14s %14s %8s %10s %10s %8s' % (
    'case', 'Ir_D47', 'Ir_D48', 'dIr%', 'est_D47', 'est_D48', 'dEst%')
print(hdr, flush=True)
print('-' * len(hdr), flush=True)
sIr = [0, 0]
sEs = [0, 0]
for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        print('%-26s MISSING' % c, flush=True); continue
    ds = [measure(b, c) for b in BINS]
    if not all(ds):
        print('%-26s FAIL' % c, flush=True); continue
    ir = [d.get('Ir', 0) for d in ds]
    es = []
    for d in ds:
        d1 = d.get('D1mr', 0) + d.get('D1mw', 0)
        dl = d.get('DLmr', 0) + d.get('DLmw', 0)
        es.append(d.get('Ir', 0) + 5 * d1 + 200 * dl)
    sIr[0] += ir[0]; sIr[1] += ir[1]
    sEs[0] += es[0]; sEs[1] += es[1]
    print('%-26s %14d %14d %+8.3f %10d %10d %+8.3f' % (
        c, ir[0], ir[1], (ir[1] / ir[0] - 1) * 100,
        es[0], es[1], (es[1] / es[0] - 1) * 100), flush=True)
print('-' * len(hdr), flush=True)
print('TOTAL Ir  D47=%d  D48=%d  d=%+.3f%%' % (
    sIr[0], sIr[1], (sIr[1] / sIr[0] - 1) * 100), flush=True)
print('TOTAL est D47=%d  D48=%d  d=%+.3f%%' % (
    sEs[0], sEs[1], (sEs[1] / sEs[0] - 1) * 100), flush=True)
print('DONE', flush=True)
