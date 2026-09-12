#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
mixradix_gain.py — 估算"允许非 2 幂 FFT 长度"能在官方测试点上省多少 FFT 工作量 (VM 上跑)

工作量代理: L * log2(L)  (蝶形总数正比于此)
约束: 误差代理 2^(2k) * L 不得超过当前设置的误差代理 (即不牺牲精度)

输出每个测试点: 当前长度 -> 可选最优长度, 省下的百分比; 以及全局加权汇总。
"""
import os
import glob
import math
import sys

IN_DIR = os.path.expanduser(
    '~/hexbench/data/mul')

GK = [19 << 4, 18 << 6, 17 << 8, 16 << 10, 15 << 12, 14 << 14,
      13 << 16, 12 << 18, 11 << 20, 10 << 22, 1 << 62]

# 允许的长度族: r * 2^m
RADICES = [int(x) for x in (sys.argv[1].split(',') if len(sys.argv) > 1
                            else ['2', '3', '5', '9', '15'])]


def pick_k(u):
    for i, lim in enumerate(GK):
        if u <= lim:
            return 19 - i
    return 9


def next_pow2(c):
    return 1 << (c - 1).bit_length()


def cands(c, errcap, k):
    out = []
    for m in range(4, 26):
        for r in RADICES:
            L = r << m
            if L >= c and (2 ** (2 * k)) * L <= errcap:
                out.append(L)
    return min(out) if out else None


def work(L):
    return L * math.log2(L)


if __name__ == '__main__':
    files = sorted(glob.glob(os.path.join(IN_DIR, '*.in')))
    print('radices = %s' % RADICES)
    print('%-16s %12s %12s %9s %8s' %
          ('case', 'work_now', 'work_mix', 'save%', 'share%'))
    tot_now = tot_mix = 0.0
    rows = []
    for f in files:
        toks = open(f).read().split()
        T = int(toks[0])
        toks = toks[1:]
        wn = wm = 0.0
        for i in range(T):
            la = len(toks[2 * i].lstrip('-'))
            lb = len(toks[2 * i + 1].lstrip('-'))
            if la <= 32 and lb <= 32:
                continue
            na, nb = -(-la // 16), -(-lb // 16)
            if min(na, nb) <= 48:
                continue
            u = na + nb
            k = pick_k(u)
            c = -(-u * 64 // k)
            Lnow = c if (c & (c - 1)) == 0 else next_pow2(c)
            errcap = (2 ** (2 * k)) * Lnow           # 不放宽精度
            Lmix = cands(c, errcap, k) or Lnow
            wn += work(Lnow)
            wm += work(Lmix)
        rows.append((os.path.basename(f)[:-3], wn, wm))
        tot_now += wn
        tot_mix += wm
    for name, wn, wm in rows:
        sv = 100.0 * (wn - wm) / wn if wn else 0.0
        sh = 100.0 * wn / tot_now if tot_now else 0.0
        print('%-16s %12.4g %12.4g %9.2f %8.2f' % (name, wn, wm, sv, sh))
    print('-' * 62)
    print('%-16s %12.4g %12.4g %9.2f' %
          ('TOTAL', tot_now, tot_mix,
           100.0 * (tot_now - tot_mix) / tot_now if tot_now else 0.0))
