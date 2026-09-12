#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
offcmp.py — 官方测试点 A/B 对比 + 逐字节一致性 (必须在 VM 上跑)

用法:
    python3 offcmp.py <binA> <binB> [reps]

对 ~/lcp/big_integer/multiplication_of_big_integers/in/*.in 逐点:
  1. 跑 A、B 各一次, 输出 md5 比对 (必须完全一致, 否则 **DIFF**)
  2. perf stat -r <reps> task-clock, ABBA 交替取最小, 打印 B/A
"""
import os
import sys
import glob
import hashlib
import subprocess

IN_DIR = os.path.expanduser(
    '~/hexbench/data/mul')


def md5_of(binp, path):
    with open(path) as fin:
        r = subprocess.run([binp], stdin=fin, stdout=subprocess.PIPE)
    return hashlib.md5(r.stdout).hexdigest()


def timed(binp, path, reps):
    cmd = ['perf', 'stat', '-x,', '-e', 'task-clock', '-r', str(reps), binp]
    with open(path) as fin, open('/dev/null', 'w') as fout:
        r = subprocess.run(cmd, stdin=fin, stdout=fout,
                           stderr=subprocess.PIPE, text=True)
    for line in r.stderr.splitlines():
        if 'task-clock' in line:
            return float(line.split(',')[0])
    return -1.0


if __name__ == '__main__':
    ba, bb = sys.argv[1], sys.argv[2]
    reps = int(sys.argv[3]) if len(sys.argv) > 3 else 3
    files = sorted(glob.glob(os.path.join(IN_DIR, '*.in')))
    print('A = %s' % ba)
    print('B = %s' % bb)
    print('%-20s %10s %10s %8s %8s' % ('case', 'A_ms', 'B_ms', 'B/A', 'same'))
    tot_a = tot_b = 0.0
    bad = 0
    for f in files:
        name = os.path.basename(f)[:-3]
        ha = md5_of(ba, f)
        hb = md5_of(bb, f)
        same = (ha == hb)
        if not same:
            bad += 1
        a1 = timed(ba, f, reps)
        b1 = timed(bb, f, reps)
        b2 = timed(bb, f, reps)
        a2 = timed(ba, f, reps)
        a = min(a1, a2)
        b = min(b1, b2)
        tot_a += a
        tot_b += b
        print('%-20s %10.2f %10.2f %8.4f %8s' %
              (name, a, b, (b / a if a > 0 else 0),
               'OK' if same else '**DIFF**'))
        sys.stdout.flush()
    print('-' * 62)
    print('%-20s %10.2f %10.2f %8.4f  bad=%d' %
          ('TOTAL', tot_a, tot_b, (tot_b / tot_a if tot_a > 0 else 0), bad))
