#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
bench_ab.py — 低噪声 A/B 计时 (必须在 VM 上跑)

用法:
    python3 bench_ab.py <binA> <binB> [K] [cpu]
        K   每个二进制的独立运行次数 (默认 11)
        cpu 绑定的核号 (默认 2)

与 offcmp.py 的区别 / 为什么噪声更低:
  1. perf -r N 报的是 **均值**, 会把偶发毛刺算进去。这里改成
     单次运行 x K, 取 **最小值** —— 加性噪声下 min 是更好的真值估计。
  2. taskset 绑定固定核, 消除核间迁移与频率域切换。
  3. A/B 交替 (ABAB...), 抵消热漂移与后台负载的慢变化。
  4. 同时报 min / p50 / spread(=p50/min), spread 大说明这次测量不可信。

判据: 只有当 |ratio-1| 明显超过同一二进制自比的底噪时, 才算真实差异。
"""
import os
import sys
import glob
import time
import hashlib
import subprocess

IN_DIR = os.path.expanduser(
    '~/hexbench/data/mul')


def run_once(binp, path, cpu):
    with open(path, 'rb') as fin, open('/dev/null', 'wb') as fout:
        t0 = time.perf_counter()
        subprocess.run(['taskset', '-c', str(cpu), binp],
                       stdin=fin, stdout=fout)
        t1 = time.perf_counter()
    return (t1 - t0) * 1000.0


def md5_of(binp, path):
    with open(path, 'rb') as fin:
        r = subprocess.run([binp], stdin=fin, stdout=subprocess.PIPE)
    return hashlib.md5(r.stdout).hexdigest()


def exp_md5(path):
    """官方 .exp 的 md5; 没有 .exp 时返回 None (退化为 A/B 互比)."""
    e = path[:-3] + '.exp'
    if not os.path.exists(e):
        return None
    with open(e, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()


def stat(v):
    v = sorted(v)
    return v[0], v[len(v) // 2]


if __name__ == '__main__':
    ba, bb = sys.argv[1], sys.argv[2]
    K = int(sys.argv[3]) if len(sys.argv) > 3 else 11
    cpu = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    files = sorted(glob.glob(os.path.join(IN_DIR, '*.in')))

    print('A = %s' % ba)
    print('B = %s' % bb)
    print('K = %d   cpu = %d' % (K, cpu))
    print('%-16s %8s %8s %8s %8s %8s %7s %-12s' %
          ('case', 'A_min', 'B_min', 'ratio', 'A_p50', 'B_p50', 'sprdB', 'vs .exp'))
    tot_a = tot_b = 0.0
    bad = 0
    worst = 1.0
    best = 1.0
    for f in files:
        name = os.path.basename(f)[:-3]
        ma, mb = md5_of(ba, f), md5_of(bb, f)
        me = exp_md5(f)
        if me is None:
            tag = 'OK' if ma == mb else '**DIFF**'
        elif ma == me and mb == me:
            tag = 'OK'
        elif ma != me and mb != me:
            tag = '**A&B!=EXP**'
        elif ma != me:
            tag = '**A!=EXP**'
        else:
            tag = '**B!=EXP**'
        same = (tag == 'OK')
        if not same:
            bad += 1
        va, vb = [], []
        # 预热一次, 让 page cache / 分支预测器进入稳态
        run_once(ba, f, cpu)
        run_once(bb, f, cpu)
        for _ in range(K):
            va.append(run_once(ba, f, cpu))
            vb.append(run_once(bb, f, cpu))
        amin, ap50 = stat(va)
        bmin, bp50 = stat(vb)
        r = bmin / amin if amin > 0 else 0.0
        worst = max(worst, r)
        best = min(best, r)
        tot_a += amin
        tot_b += bmin
        print('%-16s %8.2f %8.2f %8.4f %8.2f %8.2f %7.3f %-12s' %
              (name, amin, bmin, r, ap50, bp50,
               (bp50 / bmin if bmin > 0 else 0),
               tag))
        sys.stdout.flush()
    print('-' * 76)
    print('%-16s %8.2f %8.2f %8.4f    bad=%d  worst=%.4f best=%.4f' %
          ('TOTAL(minsum)', tot_a, tot_b,
           (tot_b / tot_a if tot_a > 0 else 0), bad, worst, best))
