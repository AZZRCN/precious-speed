#!/usr/bin/env python3
# 用 -DFFTHIST 编译并跑指定用例, 取 FFT 长度直方图 (含 2pow/fft3 区分与 N*logN 工作量)
import subprocess, os, sys, math

REMOTE = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRCS = (sys.argv[1] if len(sys.argv) > 1 else 'div_D16').split(',')
CASES = (sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03').split(',')
os.chdir(REMOTE)
os.makedirs('bin', exist_ok=True)

for src in SRCS:
    tag = 'fh_' + src
    cmd = ('g++ -O2 -std=c++23 -march=x86-64-v3 -DFFTHIST -o bin/%s src/%s.cpp' % (tag, src))
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print('COMPILE FAIL', src); print(r.stderr[-3000:]); continue
    for case in CASES:
        p = subprocess.run('./bin/%s < %s%s.in > /dev/null' % (tag, IN, case),
                           shell=True, capture_output=True, text=True)
        print('##### %s / %s' % (src, case), flush=True)
        print(p.stderr, flush=True)
        # 额外: 计算若把每个 fft3 长度换成下一个 2 幂, 工作量会变多少 (反推 radix-3 收益)
        tot3 = tot2 = 0.0
        for ln in p.stderr.splitlines():
            ln = ln.strip()
            if not ln.startswith('len='):
                continue
            try:
                a = ln.split()
                N = int(a[0].split('=')[1]); c = int(a[1].lstrip('x'))
            except Exception:
                continue
            tot3 += N * math.log2(N) * c
            N2 = 1 << (N - 1).bit_length()
            tot2 += N2 * math.log2(N2) * c
        if tot3:
            print('  [derive] 当前sum(NlogN)=%.6g ; 若全部退化到2幂=%.6g ; radix-3 已省=%.2f%%'
                  % (tot3, tot2, 100.0 * (1 - tot3 / tot2)), flush=True)
        print('----- end -----\n', flush=True)
