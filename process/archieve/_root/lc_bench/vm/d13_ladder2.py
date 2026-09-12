#!/usr/bin/env python3
"""D13 档位阶梯模拟器 v2 —— 带"非 2 幂级效率惩罚"与"级数上限"。

v1 用纯 N*log2(N) 衡量, 会高估多级 radix-3/5/7 的收益。
v2 代价模型:
    N = c * 2^k,  c = Π r_i  (r_i ∈ {3,5,7})
    cost(N) = N * [ k + Σ_i penalty[r_i] * log2(r_i) ]
即每个非 2 幂级按其"等效 radix-2 级数"计价, 再乘一个 >1 的惩罚系数
(非 2 幂蝶形 SIMD 友好度差、twiddle 更多)。

同时限制 len(非 2 幂级) <= MAXST, 用来看"多做一级值多少钱"。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
CASES = sys.argv[1:] or [
    "length_ratio_integer_00", "length_ratio_integer_01",
    "length_ratio_integer_02", "length_ratio_integer_03",
    "length_ratio_integer_04", "length_ratio_integer_05",
    "a_max_b_random_02", "a_max_b_random_01",
    "r_nearly_zero_01", "burnikel_ziegler_bound_01",
    "large_00", "medium_02",
]

REMOTE = r'''
import math, os, sys, itertools
from collections import defaultdict

PEN = {3: 1.15, 5: 1.25, 7: 1.35}   # 非 2 幂级效率惩罚
MINBLK = 64                          # 最内层 2 幂块的最小长度

def gen(radices, maxst):
    """返回 [(c, [r...])], c = Π r"""
    out = [(1, ())]
    for n in range(1, maxst + 1):
        for combo in itertools.combinations_with_replacement(radices, n):
            c = 1
            for r in combo:
                c *= r
            out.append((c, combo))
    # 同一个 c 只保留代价最小的分解
    best = {}
    for c, combo in out:
        cost = sum(PEN[r] * math.log2(r) for r in combo)
        if c not in best or cost < best[c][0]:
            best[c] = (cost, combo)
    return sorted((c, v[0]) for c, v in best.items())

def make_ceil(table):
    def f(n):
        best = None
        for c, extra in table:
            v = c
            k = 0
            while v < n:
                v <<= 1
                k += 1
            # 需要 blk = v/c = 2^k >= MINBLK (c>1 时)
            if c > 1 and (1 << k) < MINBLK:
                continue
            cost = v * (k + extra)
            if best is None or cost < best[0]:
                best = (cost, v)
        return best
    return f

LAD = {}
LAD["P2"]      = make_ceil(gen([], 0))
LAD["P23"]     = make_ceil(gen([3], 1))
LAD["3x2"]     = make_ceil(gen([3], 2))          # {1,3,9}
LAD["3x3"]     = make_ceil(gen([3], 3))          # {1,3,9,27}
LAD["35x2"]    = make_ceil(gen([3, 5], 2))       # {1,3,5,9,15,25}
LAD["357x2"]   = make_ceil(gen([3, 5, 7], 2))
LAD["357x3"]   = make_ceil(gen([3, 5, 7], 3))
LAD["357x5"]   = make_ceil(gen([3, 5, 7], 5))

def ideal(n):
    return (n * math.log2(n), n)

CASES = sys.argv[1:]
names = list(LAD)
print(f"{'case':<28}" + "".join(f"{k:>9}" for k in names) + f"{'IDEAL':>9}")
print("-" * (28 + 9 * (len(names) + 1)))
grand = defaultdict(float)
picks = defaultdict(lambda: defaultdict(int))
for case in CASES:
    path = f"/tmp/ceil_{case}.csv"
    if not os.path.exists(path):
        print(f"{case:<28} (missing)")
        continue
    tot = defaultdict(float)
    with open(path) as f:
        next(f)
        for line in f:
            tag, need, used, cnt = line.split(",")
            need, cnt = int(need), int(cnt)
            if need < 2:
                continue
            tot["IDEAL"] += ideal(need)[0] * cnt
            for name in names:
                cost, v = LAD[name](need)
                tot[name] += cost * cnt
                if name == "357x5" and need > 20000:
                    picks[name][(need, v)] += cnt
    base = tot["P23"]
    row = f"{case:<28}"
    for name in names + ["IDEAL"]:
        grand[name] += tot[name]
        row += f"{tot[name]/base*100:8.1f}%"
    print(row)
print("-" * (28 + 9 * (len(names) + 1)))
row = f"{'GRAND (vs P23=100)':<28}"
for name in names + ["IDEAL"]:
    row += f"{grand[name]/grand['P23']*100:8.1f}%"
print(row)

print("\n--- 357x5 对大变换(need>20000)的选档 (need -> used, 前 24) ---")
agg = defaultdict(int)
for k, v in picks["357x5"].items():
    agg[k] += v
for (need, v), cnt in sorted(agg.items(), key=lambda kv: -kv[0][0])[:24]:
    fac = v
    e2 = 0
    while fac % 2 == 0:
        fac //= 2
        e2 += 1
    print(f"  need={need:<9} used={v:<9} = {fac}*2^{e2:<3} waste={100*(v/need-1):5.1f}%  x{cnt}")
'''

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")
tmp = os.path.join(PS, "lc_bench", "vm", "_ladder_sim2.py")
with open(tmp, "w", encoding="utf-8") as f:
    f.write(REMOTE)
put(tmp, "/tmp/ladder_sim2.py")
rc, out, err = run("python3 /tmp/ladder_sim2.py " + " ".join(CASES), timeout=900)
print(out)
