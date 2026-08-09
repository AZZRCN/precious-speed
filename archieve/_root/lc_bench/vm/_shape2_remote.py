#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""测量各用例内每组 (A,B) 的 limb 形状(base 10^4)与 absDivRem 分派分支。
   LC 格式: 第一行 T, 之后 T 行, 每行 "A B"。
   汇总: 按分支聚合 sum(qn*limbB) 作为 schoolbook 工作量代理。"""
import os
import sys

IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
cases = sys.argv[1].split(",") if len(sys.argv) > 1 else sorted(
    f[:-3] for f in os.listdir(IN) if f.endswith(".in"))
TOPK = int(sys.argv[2]) if len(sys.argv) > 2 else 3


def branch(la, lb):
    if la < lb:
        return "trivial"
    if lb == 1:
        return "divRem1"
    if lb <= 64 or (la - lb) <= 64:
        return "BASIC"
    if la < lb * 2:
        return "Newton1"
    return "Mu"


for c in cases:
    p = os.path.join(IN, c + ".in")
    if not os.path.exists(p):
        continue
    with open(p, "r") as fh:
        data = fh.read().split()
    t = int(data[0])
    rows = []
    agg = {}
    for i in range(t):
        a = data[1 + 2 * i].lstrip("-")
        b = data[2 + 2 * i].lstrip("-")
        la, lb = (len(a) + 3) // 4, (len(b) + 3) // 4
        qn = la - lb + 1 if la >= lb else 0
        br = branch(la, lb)
        work = qn * lb if br == "BASIC" else 0
        rows.append((work, la, lb, qn, br))
        d = agg.setdefault(br, [0, 0])
        d[0] += 1
        d[1] += work
    rows.sort(reverse=True)
    print("=" * 96)
    print("%-30s T=%d   %s" % (
        c, t, "  ".join("%s:n=%d,work=%.2fM" % (k, v[0], v[1] / 1e6)
                        for k, v in sorted(agg.items()))))
    for w, la, lb, qn, br in rows[:TOPK]:
        print("    limbA=%8d limbB=%8d qn=%8d  %-8s schoolbook_work=%.2fM" % (
            la, lb, qn, br, w / 1e6))
