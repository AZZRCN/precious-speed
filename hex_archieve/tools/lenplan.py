#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# AZZRCN
# https://github.com/AZZRCN
"""
lenplan.py — FFT 长度/精度联合规划器 (VM 上跑, 读 ~/hexbench/data/mul)

回答一个问题: 在不牺牲精度的前提下, 换用「更细的长度粒度」和/或
「平衡(有符号)数字表示」, 每个官方测试点的 FFT 工作量能降多少?

模型
----
* 系数位宽 k, 系数个数 c = ceil(u*64 / k), u = na+nb (limb)
* 变换长度 L 必须 >= c, 且形如 r * 2^m (r 属于 RADICES)
* 误差代理: 2^(2k) * L <= BUDGET
    - 无符号数字 (现状): 系数 in [0, 2^k),   BUDGET = 2^48   (= 现有 gk 表的不变量)
    - 平衡数字:         系数 in [-2^(k-1), 2^(k-1)), 误差项降 4x, BUDGET = 2^50
* 工作量代理: 3 * L * log2(L)   (DIF a + DIF b + DIT, 正比于蝶形数)

用法
----
    python3 lenplan.py                       # 四种方案对照
    python3 lenplan.py 2,3,5,9,15,27         # 指定可用基
"""
import os
import glob
import math
import sys
from collections import defaultdict

IN_DIR = os.path.expanduser('~/hexbench/data/mul')

GK = [19 << 4, 18 << 6, 17 << 8, 16 << 10, 15 << 12, 14 << 14,
      13 << 16, 12 << 18, 11 << 20, 10 << 22, 1 << 62]

RADICES = sorted({int(x) for x in (sys.argv[1].split(',') if len(sys.argv) > 1
                                   else ['2', '3', '5', '9', '15', '27', '25', '45'])})

BF_LIMB = 48          # mb <= 48 走 comba, 不进 FFT
BUDGET_PLAIN = 2 ** 48
BUDGET_BAL = 2 ** 50


def pick_k_now(u):
    for i, lim in enumerate(GK):
        if u <= lim:
            return 19 - i
    return 9


def next_pow2(c):
    return 1 << (c - 1).bit_length()


def plan(u, budget, radices):
    """在 budget 下枚举 (k, L), 取 L*log2(L) 最小者。返回 (k, L, work)."""
    best = None
    for k in range(8, 25):
        c = (u * 64 + k - 1) // k
        for m in range(4, 27):
            for r in radices:
                L = r << m
                if L < c:
                    continue
                if (2 ** (2 * k)) * L > budget:
                    continue
                w = L * math.log2(L)
                if best is None or w < best[2]:
                    best = (k, L, w)
                break          # r 递增, 同 m 下第一个够用的最小
    return best


def now(u):
    k = pick_k_now(u)
    c = (u * 64 + k - 1) // k
    L = c if (c & (c - 1)) == 0 else next_pow2(c)
    return k, L, L * math.log2(L)


def shapes(path):
    toks = open(path).read().split()
    T = int(toks[0])
    toks = toks[1:]
    out = []
    for i in range(T):
        a, b = toks[2 * i], toks[2 * i + 1]
        la = len(a) - (1 if a[0] == '-' else 0)
        lb = len(b) - (1 if b[0] == '-' else 0)
        ma, mb = (la + 15) // 16, (lb + 15) // 16
        if ma < mb:
            ma, mb = mb, ma
        out.append((ma, mb))
    return out


SCHEMES = [
    ('now      pow2 /2^48', None, None),
    ('mix      r*2^m/2^48', BUDGET_PLAIN, None),
    ('bal      pow2 /2^50', BUDGET_BAL, [2]),
    ('bal+mix  r*2^m/2^50', BUDGET_BAL, None),
]

if __name__ == '__main__':
    files = sorted(glob.glob(os.path.join(IN_DIR, '*.in')))
    print('radices = %s' % RADICES)
    print('%-16s %11s %11s %11s %11s' % ('case', 'now', 'mix', 'bal', 'bal+mix'))
    tot = defaultdict(float)
    detail = {}
    for f in files:
        name = os.path.basename(f)[:-3]
        row = defaultdict(float)
        klmax = {}
        for ma, mb in shapes(f):
            if mb == 0 or mb <= BF_LIMB:
                continue
            u = ma + mb
            for tag, budget, rad in SCHEMES:
                if budget is None:
                    k, L, w = now(u)
                else:
                    k, L, w = plan(u, budget, rad if rad else RADICES)
                row[tag] += 3.0 * w
                if u > klmax.get(tag, (0,))[0]:
                    klmax[tag] = (u, k, L)
        for tag, _, _ in SCHEMES:
            tot[tag] += row[tag]
        detail[name] = klmax
        print('%-16s %11.4g %11.4g %11.4g %11.4g' %
              (name, row[SCHEMES[0][0]], row[SCHEMES[1][0]],
               row[SCHEMES[2][0]], row[SCHEMES[3][0]]))
    print('-' * 66)
    base = tot[SCHEMES[0][0]]
    print('%-16s %11.4g %11.4g %11.4g %11.4g' %
          ('TOTAL', base, tot[SCHEMES[1][0]], tot[SCHEMES[2][0]],
           tot[SCHEMES[3][0]]))
    print('%-16s %10s %10.2f%% %10.2f%% %10.2f%%' %
          ('save vs now', '-',
           100 * (1 - tot[SCHEMES[1][0]] / base),
           100 * (1 - tot[SCHEMES[2][0]] / base),
           100 * (1 - tot[SCHEMES[3][0]] / base)))
    print()
    print('== 最大 u 的 (k, L) 选择 ==')
    print('%-16s %9s %-22s %-22s %-22s %-22s' %
          ('case', 'u_max', *[s[0] for s in SCHEMES]))
    for name in sorted(detail):
        d = detail[name]
        if not d:
            continue
        u = d[SCHEMES[0][0]][0]
        cells = []
        for tag, _, _ in SCHEMES:
            _, k, L = d[tag]
            cells.append('k=%-3d L=%-9d' % (k, L))
        print('%-16s %9d %-22s %-22s %-22s %-22s' % (name, u, *cells))
