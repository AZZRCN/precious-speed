#!/usr/bin/env python3
"""D13 档位阶梯模拟器。

用 CEILHIST 导出的真实 (need -> used) 直方图, 离线评估各种候选阶梯
能把 FFT 总工作量 sum(N*log2 N) 压到多少。

候选阶梯 (c 为奇数乘子, 长度 = c * 2^k):
  P2      : c in {1}                    —— D11 及以前
  P23     : c in {1,3}                  —— D12 (已 AC 57ms)
  P235    : c in {1,3,5}                —— 需要 radix-5 顶层
  P2357   : c in {1,3,5,7}              —— 再加 radix-7 顶层
  P2_SM   : c in 所有 7-smooth 奇数      —— 通用混合基 (FFTW 式)
  IDEAL   : 无粒度损失 (下界)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
CASES = sys.argv[1:] or [
    "length_ratio_integer_00", "length_ratio_integer_01",
    "length_ratio_integer_02", "length_ratio_integer_03",
    "length_ratio_integer_04", "length_ratio_integer_05",
    "a_max_b_random_02", "a_max_b_random_01",
    "r_nearly_zero_01", "burnikel_ziegler_bound_01",
    "large_00", "medium_02",
]

REMOTE = r"""
import math, os, sys
from collections import defaultdict

def odd_mults(maxset):
    return sorted(maxset)

def smooth_odds(limit=4096):
    s = []
    for c in range(1, limit, 2):
        x = c
        for p in (3, 5, 7):
            while x % p == 0:
                x //= p
        if x == 1:
            s.append(c)
    return s

LADDERS = {
    "P2":    [1],
    "P23":   [1, 3],
    "P235":  [1, 3, 5],
    "P2357": [1, 3, 5, 7],
    "P2SM":  smooth_odds(),
}
MINLEN = 192   # 非 2 幂顶层的最小长度限制 (与 FFT3_MIN 一致)

def ceil_with(n, odds):
    best = None
    for c in odds:
        # 找最小的 c*2^k >= n
        if c == 1:
            v = 1
            while v < n:
                v <<= 1
        else:
            v = c
            while v < n:
                v <<= 1
            # c*2^k 形式下 k 可为 0, 但要求 >= MINLEN 且是 c*2^k
            if v < MINLEN:
                continue
        if best is None or v < best:
            best = v
    return best

def work(n):
    return n * math.log2(n) if n >= 2 else 0.0

CASES = sys.argv[1:]
print(f"{'case':<28}" + "".join(f"{k:>11}" for k in LADDERS) + f"{'IDEAL':>11}")
print("-" * (28 + 11 * (len(LADDERS) + 1)))
grand = defaultdict(float)
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
            tot["IDEAL"] += work(need) * cnt
            for name, odds in LADDERS.items():
                tot[name] += work(ceil_with(need, odds)) * cnt
    base = tot["P23"]
    row = f"{case:<28}"
    for name in list(LADDERS) + ["IDEAL"]:
        grand[name] += tot[name]
        row += f"{tot[name]/base*100:10.1f}%"
    print(row)
print("-" * (28 + 11 * (len(LADDERS) + 1)))
row = f"{'GRAND (vs P23=100)':<28}"
for name in list(LADDERS) + ["IDEAL"]:
    row += f"{grand[name]/grand['P23']*100:10.1f}%"
print(row)
"""

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")
rc, out, err = run(
    "cd ~/divbench/src && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DCEILHIST -o ../bin/w_d13 div_D13.cpp "
    "2>&1 | tail -20 && echo BUILD_DONE", timeout=1200)
print(out)
if "BUILD_DONE" not in out:
    sys.exit("build failed")

for c in CASES:
    run(f"cd ~/divbench && CEILHIST_CSV=/tmp/ceil_{c}.csv ./bin/w_d13 "
        f"< {IN}/{c}.in > /dev/null 2>/dev/null; wc -l /tmp/ceil_{c}.csv", timeout=600)

with open(os.path.join(PS, "lc_bench", "vm", "_ladder_sim.py"), "w", encoding="utf-8") as f:
    f.write(REMOTE)
put(os.path.join(PS, "lc_bench", "vm", "_ladder_sim.py"), "/tmp/ladder_sim.py")
rc, out, err = run("python3 /tmp/ladder_sim.py " + " ".join(CASES), timeout=900)
print(out)
