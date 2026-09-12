#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FFT 档位阶梯离线模拟 —— 用真实需求分布评估各档位集合的收益。

输入: _ceilhist_all.txt (CEILHIST_CSV 导出, 格式 tag,need,used,cnt)
      tag 0=lin(线性卷积) 1=mn(Newton) 2=cycm(循环卷积模数)
代价: N*log2(N)  (与 div.cpp 内 CEILHIST 探针口径一致)

为什么要模拟: 加一档 radix 要实现对应蝶形, 代价高。先用真实分布算清
              每档能回收多少 N*logN, 避免重蹈 D40 radix-7 (无统计差异) 的覆辙。
"""
import io
import math
import sys
from collections import defaultdict

SRC = "D:/precious_speed/lc_bench/_ceilhist_all.txt"


def load():
    data = defaultdict(list)          # case -> [(tag, need, used, cnt)]
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


def ladder(mults, nmax, minlen):
    """生成档位集合: 所有 m*2^k (m in mults), 且 m>1 的档位需 >= minlen[m]"""
    s = set()
    for m in mults:
        lo = minlen.get(m, 0)
        v = m
        while v <= nmax:
            if v >= lo:
                s.add(v)
            v *= 2
    return sorted(s)


def smooth(nmax, primes):
    """所有 primes-smooth 数 (理论下界参考)"""
    s = [1]
    for p in primes:
        ns = []
        for v in s:
            while v <= nmax:
                ns.append(v)
                v *= p
        s = ns
    return sorted(set(x for x in s if x >= 2))


def cost(n):
    return n * math.log2(n) if n > 1 else 0.0


def pick(L, need):
    import bisect
    i = bisect.bisect_left(L, need)
    return L[i] if i < len(L) else None


# radix-r 蝶形要求每块 M = float_len/(2r) >= 32  -> float_len >= 64r
MINLEN = {3: 192, 5: 320, 7: 448, 9: 576, 15: 960}

LADDERS = {
    "2 only          ": [2],
    "2,3   (=D39)    ": [2, 3],
    "2,3,7 (=D40)    ": [2, 3, 7],
    "2,3,5           ": [2, 3, 5],
    "2,3,5,7         ": [2, 3, 5, 7],
    "2,3,5,7,9       ": [2, 3, 5, 7, 9],
    "2,3,5,7,9,15    ": [2, 3, 5, 7, 9, 15],
}

if __name__ == "__main__":
    data = load()
    nmax = 1 << 26
    sets = {k: ladder(v, nmax, MINLEN) for k, v in LADDERS.items()}
    sets["5-smooth (bound)"] = smooth(nmax, [2, 3, 5])

    only_tag = None
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        only_tag = int(sys.argv[1])
        print(f"(only tag={only_tag})")

    names = list(sets.keys())
    print(f"{'case':<28}{'ideal':>12}  " + "".join(f"{n[:16]:>18}" for n in names))
    print("-" * (40 + 18 * len(names)))

    grand = defaultdict(float)
    gideal = 0.0
    for case in sorted(data):
        rows = [r for r in data[case] if only_tag is None or r[0] == only_tag]
        if not rows:
            continue
        ideal = sum(cnt * cost(need) for _, need, _, cnt in rows)
        gideal += ideal
        line = f"{case:<28}{ideal:>12.4g}  "
        for n in names:
            L = sets[n]
            tot = 0.0
            for _, need, _, cnt in rows:
                u = pick(L, need)
                tot += cnt * cost(u if u else need)
            grand[n] += tot
            line += f"{tot / ideal:>17.4f} "
        print(line)
    print("-" * (40 + 18 * len(names)))
    line = f"{'GRAND':<28}{gideal:>12.4g}  "
    for n in names:
        line += f"{grand[n] / gideal:>17.4f} "
    print(line)
    base = grand["2,3   (=D39)    "]
    print("\n相对当前 D39 档位(2,3)的净收益:")
    for n in names:
        print(f"  {n}  {grand[n] / base:.4f}   ({100 * (grand[n] / base - 1):+.2f}%)")
