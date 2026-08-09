#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FFT 档位阶梯模拟 v3 —— 参数化 radix 最小尺寸门槛。

v2 的关键盲点: 把 radix-R 的可用下限硬设为 n >= 64*R, 于是
  - radix-3 最小档 192 (与源码 FFT3_MIN 巧合一致)
  - radix-5 最小档 320
而源码 real_dot_binrev3 的真实约束只是 blk = 2m >= 16, 即 float_len = 6m >= 48。
换言之现有 FFT3_MIN=192 比真实可行下限保守 4 倍, 小尺寸 FFT 全被赶回 2 幂档。
本脚本把下限参数化, 量化"仅调低门槛"(零新代码) 与"再加 radix-5"的收益。

cost(R * 2^k) = n * (k + log2(R) * P_R)   # P_R = radix-R 每 bit 效率惩罚
"""
import io
import math
import sys
import bisect
from collections import defaultdict

SRC = "D:/precious_speed/lc_bench/_ceilhist_all.txt"
P3 = 1.19   # radix-3 每 bit 惩罚 (v2 标定值)
P5 = 1.25   # radix-5 每 bit 惩罚 (保守: 比 radix-3 再差一点)


def load():
    data = defaultdict(list)
    cur = None
    for line in io.open(SRC, encoding="utf-8"):
        s = line.strip()
        if s.startswith("##FILE"):
            cur = s.split()[1].replace("ch_", "")
            continue
        if not s or "," not in s or cur is None:
            continue
        p = s.split(",")
        if len(p) != 4 or not p[0].lstrip("-").isdigit():
            continue
        data[cur].append(tuple(int(x) for x in p))
    return data


def build(rules, nmax=1 << 24):
    """rules: list of (R, min_len, penalty). R=1 表示纯 2 幂。
    返回 [(len, cost_per_unit_total)] 排序表。"""
    tab = {}
    for R, minlen, pen in rules:
        n = R if R > 1 else 1
        k = 0
        while n * (1 << k) <= nmax:
            L = n * (1 << k)
            if L >= minlen and L >= 16:
                c = L * (k + (math.log2(R) * pen if R > 1 else 0))
                if L not in tab or c < tab[L]:
                    tab[L] = c
            k += 1
    keys = sorted(tab)
    return keys, tab


def evaluate(data, rules, cases):
    keys, tab = build(rules)
    out = {}
    grand = 0.0
    for c in cases:
        tot = 0.0
        for _kind, need, _used, cnt in data[c]:
            i = bisect.bisect_left(keys, need)
            if i >= len(keys):
                L = keys[-1]
            else:
                L = keys[i]
            tot += tab[L] * cnt
        out[c] = tot
        grand += tot
    out["GRAND"] = grand
    return out


def ideal(data, cases):
    out = {}
    g = 0.0
    for c in cases:
        t = 0.0
        for _k, need, _u, cnt in data[c]:
            t += (need * math.log2(need) if need > 1 else 0) * cnt
        out[c] = t
        g += t
    out["GRAND"] = g
    return out


def main():
    data = load()
    cases = sorted(data.keys())
    idl = ideal(data, cases)

    schemes = [
        ("cur 3^1 min192",      [(1, 2, 0), (3, 192, P3)]),
        ("3^1 min96",           [(1, 2, 0), (3, 96, P3)]),
        ("3^1 min48",           [(1, 2, 0), (3, 48, P3)]),
        ("3^1 min48 +5 min320", [(1, 2, 0), (3, 48, P3), (5, 320, P5)]),
        ("3^1 min48 +5 min80",  [(1, 2, 0), (3, 48, P3), (5, 80, P5)]),
        ("3^<=2 min48",         [(1, 2, 0), (3, 48, P3), (9, 48, P3)]),
        ("3^<=2 min48 +5 min80", [(1, 2, 0), (3, 48, P3), (9, 48, P3),
                                  (5, 80, P5), (15, 240, P5)]),
    ]
    res = [(name, evaluate(data, rules, cases)) for name, rules in schemes]

    hdr = "%-28s %11s" % ("case", "ideal")
    for name, _ in res:
        hdr += " %-22s" % name
    print(hdr)
    print("-" * len(hdr))
    for c in cases + ["GRAND"]:
        line = "%-28s %11.4g" % (c, idl[c])
        for _n, r in res:
            line += " %-22.4f" % (r[c] / idl[c])
        print(line)
    print()
    base = res[0][1]["GRAND"]
    print("相对当前档位 (cur 3^1 min192) 的净收益:")
    for n, r in res:
        v = r["GRAND"] / base
        print("  %-24s %.4f   (%+.2f%%)" % (n, v, (v - 1) * 100))
    print()
    print("按 case 的净收益 (相对 cur):")
    b = res[0][1]
    print("%-28s" % "case" + "".join(" %-22s" % n for n, _ in res[1:]))
    for c in cases:
        line = "%-28s" % c
        for _n, r in res[1:]:
            line += " %-22s" % ("%+.2f%%" % ((r[c] / b[c] - 1) * 100))
        print(line)


if __name__ == "__main__":
    main()
