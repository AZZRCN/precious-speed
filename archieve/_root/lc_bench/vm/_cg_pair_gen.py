#!/usr/bin/env python3
import subprocess, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
A = 'd14'
B = 'd15'
CASES = ['length_ratio_integer_02', 'length_ratio_integer_03', 'length_ratio_integer_00', 'length_ratio_integer_01', 'a_max_b_random_02', 'large_01']

def ir(b, c):
    outf = '/tmp/cg_%s_%s.out' % (b, c)
    cmd = ('valgrind --tool=callgrind --cache-sim=no --branch-sim=no '
           '--callgrind-out-file=' + outf + ' ./bin/' + b +
           ' < ' + IN + c + '.in > /dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd='/home/azzr/divbench').returncode != 0:
        return None
    tot = None
    with open(outf) as f:
        for line in f:
            if line.startswith('summary:') or line.startswith('totals:'):
                tot = int(line.split()[1])
    return tot

print('%-30s %14s %14s %9s' % ('case', A + ' Ir', B + ' Ir', 'B/A'), flush=True)
print('-' * 72, flush=True)
rows = []
for c in CASES:
    if not os.path.exists(IN + c + '.in'):
        print(c + ': MISSING', flush=True); continue
    va, vb = ir(A, c), ir(B, c)
    if va and vb:
        rows.append((c, va, vb, vb / va))
        print('%-30s %14d %14d %9.4f' % (c, va, vb, vb / va), flush=True)
    else:
        print(c + ': FAIL', flush=True)
print('-' * 72, flush=True)
if rows:
    ma = max(rows, key=lambda r: r[1])
    mb = max(rows, key=lambda r: r[2])
    print('MAX-Ir  %s: %s (%d)' % (A, ma[0], ma[1]), flush=True)
    print('MAX-Ir  %s: %s (%d)' % (B, mb[0], mb[2]), flush=True)
    print('bottleneck shift: %.4f' % (mb[2] / ma[1]), flush=True)
print('DONE', flush=True)
