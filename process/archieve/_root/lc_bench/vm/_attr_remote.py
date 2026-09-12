#!/usr/bin/env python3
# lri_03 全程序 Ir 归因: 函数级 (自身 Ir) 排行 + 分类聚合
import subprocess, os, sys, re

REMOTE = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BIN = sys.argv[1] if len(sys.argv) > 1 else 'd16'
CASES = sys.argv[2].split(',') if len(sys.argv) > 2 else ['length_ratio_integer_03']
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'

for case in CASES:
    outf = '/tmp/attr_%s_%s.out' % (BIN, case)
    cmd = ('valgrind --tool=callgrind ' + CACHE + ' --callgrind-out-file=' + outf +
           ' ./bin/' + BIN + ' < ' + IN + case + '.in >/dev/null 2>/dev/null')
    if subprocess.run(cmd, shell=True, cwd=REMOTE).returncode != 0:
        print('RUN FAIL', BIN, case)
        continue
    r = subprocess.run(['callgrind_annotate', '--threshold=99.9',
                        '--show=Ir,D1mr,D1mw,DLmr,DLmw', outf],
                       capture_output=True, text=True)
    txt = r.stdout
    print('##### %s / %s' % (BIN, case), flush=True)
    # 保留原始输出以便人工核对
    keep = False
    lines = []
    for ln in txt.splitlines():
        if 'file:function' in ln or re.match(r'^\s*[\d,]+\s+[\d,]+', ln):
            keep = True
        if keep:
            lines.append(ln)
    print('\n'.join(lines[:70]), flush=True)
    print('----- end raw -----\n', flush=True)
