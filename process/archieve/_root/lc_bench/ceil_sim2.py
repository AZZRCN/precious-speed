#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FFT 档位阶梯离线模拟 v2 —— 嵌套 radix-3 (3^b*2^k) 的可实现收益。

与 v1 的三点差别 (v1 的 -11.78% 是无约束上界, 不可实现):
  1) 嵌套约束: R=3^b 需要最内层 2 幂子块 M' >= 32 复数 -> FL >= 64*R
  2) 效率惩罚: radix-3 级每 bit 比 radix-4 贵 P3 (默认 1.19)
     cost(3^b*2^k) = n * (k + b*log2(3)*P3)
  3) 只枚举 3-smooth (复用现成蝶形, 零新代码), 5/7 仅作对照

用法: python ceil_sim2.py [tag] [--p3 1.19]
"""
import io
import math
import sys
import bisect
from collections import defaultdict

SRC = "D:/precious_speed/lc_bench/_ceilhist_all.txt"


def load():
    data = defaultdict(list)
    cur = None
    for line in io.open(SRC, encoding="utf-8"):
        line = line.strip()
        if line.startswith("##FILE"):
            cur = line.split()[1].replace("ch_", "")
            continue
        if not line or "," not in line or cur is None:
            continue
        p = line.split(",")
        if len(p) != 4 or not p[0].lstrip("-").isdigit():
            continue
        data[cur].append((int(p[0]), int(p[1]), int(p[2]), int(p[3])))
    return data


def factor(n):
    """n -> (b3, b5, b7, k) 使 n = 3^b3*5^b5*7^b7*2^k, 非 smooth 返回 None"""
    b = {3: 0, 5: 0, 7: 0}
    for p in (3, 5, 7):
        while n % p == 0:
            n //= p
            b[p] += 1
    k = 0
    while n % 2 == 0:
        n //= 2
        k += 1
    if n != 1:
        return None
    return b[3], b[5], b[7], k


def build(mults, nmax):
    """mults: 允许的奇因子集合 (1 表示纯 2 幂)。返回排序档位表。
    约束: 奇因子 R 的档位需 n >= 64*R (最内 2 幂块 >= 32 复数)"""
    s = set()
    for R in mults:
        v = R * 64 if R > 1 else 2
        while v <= nmax:
            s.add(v)
            v *= 2
    return sorted(s)


def mkcost(p3, p5, p7):
    lg3, lg5, lg7 = math.log2(3), math.log2(5), math.log2(7)

    def cost(n):
        if n <= 1:
            return 0.0
        f = factor(n)
        if f is None:                      # 非 smooth: 只用于 ideal 口径
            return n * math.log2(n)
        b3, b5, b7, k = f
        return n * (k + b3 * lg3 * p3 + b5 * lg5 * p5 + b7 * lg7 * p7)

    return cost


def pick(L, need):
    i = bisect.bisect_left(L, need)
    return L[i] if i < len(L) else None


# 3-smooth 奇因子阶梯 (逐级加深嵌套层数 b)
POW3 = [1, 3, 9, 27, 81, 243, 729]

LADDERS = {
    "2only          ": [1],
    "3^1 (=D39)     ": [1, 3],
    "3^<=2 (9)      ": [1, 3, 9],
    "3^<=3 (27)     ": [1, 3, 9, 27],
    "3^<=4 (81)     ": [1, 3, 9, 27, 81],
    "3^<=5 (243)    ": [1, 3, 9, 27, 81, 243],
    "3^<=6 (729)    ": POW3,
    "3^1 +5         ": [1, 3, 5],
    "3^<=4 +5,15,45 ": [1, 3, 9, 27, 81, 5, 15, 45, 135, 405],
}

if __name__ == "__main__":
    args = [a for a in sys.argv[1:]]
    p3 = 1.19
    if "--p3" in args:
        p3 = float(args[args.index("--p3") + 1])
        del args[args.index("--p3"):args.index("--p3") + 2]
    only_tag = int(args[0]) if args and args[0].isdigit() else None

    cost = mkcost(p3, 1.25, 1.30)
    data = load()
    nmax = 1 << 27
    sets = {k: build(v, nmax) for k, v in LADDERS.items()}

    names = list(sets.keys())
    print(f"P3(radix-3 每bit惩罚)={p3}" + (f"  (only tag={only_tag})" if only_tag is not None else ""))
    print(f"{'case':<26}{'ideal':>11} " + "".join(f"{n[:15]:>16}" for n in names))
    print("-" * (37 + 16 * len(names)))

    grand = defaultdict(float)
    gideal = 0.0
    for case in sorted(data):
        rows = [r for r in data[case] if only_tag is None or r[0] == only_tag]
        if not rows:
            continue
        ideal = sum(cnt * n * math.log2(n) for _, n, _, cnt in rows if n > 1)
        gideal += ideal
        line = f"{case:<26}{ideal:>11.4g} "
        for nm in names:
            L = sets[nm]
            tot = 0.0
            for _, need, _, cnt in rows:
                u = pick(L, need)
                tot += cnt * cost(u if u else need)
            grand[nm] += tot
            line += f"{tot / ideal:>15.4f} "
        print(line)
    print("-" * (37 + 16 * len(names)))
    line = f"{'GRAND':<26}{gideal:>11.4g} "
    for nm in names:
        line += f"{grand[nm] / gideal:>15.4f} "
    print(line)

    base = grand["3^1 (=D39)     "]
    print("\n相对当前 D39 档位的净收益 (含 radix-3 效率惩罚):")
    for nm in names:
        print(f"  {nm}  {grand[nm] / base:.4f}   ({100 * (grand[nm] / base - 1):+.2f}%)")
