#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
offshape.py — 统计官方测试点的形状 (VM 上跑)

对每个 .in 输出: T、总 hex 字符数、la/lb 分布 (min/中位/max)、
以及每个 case 折算出的 limb 数 u=ceil(la/16)+ceil(lb/16)、k、FFT 长度 lm(旧/新)。
用于判断某个优化到底命中了多少实际负载。
"""
import os
import glob
import sys

IN_DIR = os.path.expanduser(
    '~/hexbench/data/mul')

GK = [19 << 4, 18 << 6, 17 << 8, 16 << 10, 15 << 12, 14 << 14,
      13 << 16, 12 << 18, 11 << 20, 10 << 22, 1 << 62]


def pick_k(u):
    for i, lim in enumerate(GK):
        if u <= lim:
            return 19 - i
    return 10


def lm_old(u, k):
    c = u * 64 // k + 1
    return 1 << (c.bit_length())          # 2 << (31-clz) == 1<<(bitlen)


def lm_new(u, k):
    c = -(-(u * 64) // k)
    return c if (c & (c - 1)) == 0 else (1 << c.bit_length())


if __name__ == '__main__':
    files = sorted(glob.glob(os.path.join(IN_DIR, '*.in')))
    print('%-16s %7s %10s %9s %9s %9s %7s %8s' %
          ('case', 'T', 'hexTotal', 'la_max', 'lb_max', 'fftWork', 'saved%', 'bfShare'))
    for f in files:
        lines = open(f).read().split()
        T = int(lines[0])
        toks = lines[1:]
        work_old = work_new = 0
        bf_ops = 0
        la_max = lb_max = 0
        hex_tot = 0
        for i in range(T):
            a = toks[2 * i].lstrip('-')
            b = toks[2 * i + 1].lstrip('-')
            la, lb = len(a), len(b)
            hex_tot += la + lb
            la_max = max(la_max, la)
            lb_max = max(lb_max, lb)
            na, nb = -(-la // 16), -(-lb // 16)
            ma, mb = max(na, nb), min(na, nb)
            if la <= 32 and lb <= 32:
                continue
            if mb <= 48:
                bf_ops += ma * mb
                continue
            u = na + nb
            k = pick_k(u)
            lo, ln = lm_old(u, k), lm_new(u, k)
            work_old += lo * max(1, lo.bit_length() - 1)
            work_new += ln * max(1, ln.bit_length() - 1)
        sv = (100.0 * (work_old - work_new) / work_old) if work_old else 0.0
        print('%-16s %7d %10d %9d %9d %9.3g %7.2f %8.3g' %
              (os.path.basename(f)[:-3], T, hex_tot, la_max, lb_max,
               work_old, sv, bf_ops))
