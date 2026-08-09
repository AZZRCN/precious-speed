# -*- coding: utf-8 -*-
"""直接跑已编译的 CEILHIST bin, 列出 need 谱, 并对每一档回答:
   "要把浪费压到 <=5%, 需要哪个 odd radix?"

对每个 need N, 枚举 odd m in {3,5,7,9,11,13,15,21,25,33,35} 与 2^j,
找 >= N 的最小 m*2^j, 报出最优 m 与届时浪费。
按 NlogN share 排序 —— 只有 share 大的档位值得为它加 codelet。

用法(远端): python3 needspec.py <bin> <case>
"""
import math
import re
import subprocess
import sys
import collections

BIN = sys.argv[1] if len(sys.argv) > 1 else "d39ch"
CASE = sys.argv[2] if len(sys.argv) > 2 else "a_max_b_random_01"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in/%s.in" % CASE

p = subprocess.run("cd /home/azzr/divbench && ./bin/%s < %s > /dev/null" % (BIN, IN),
                   shell=True, capture_output=True, text=True)
err = p.stderr

ODDS = [1, 3, 5, 7, 9, 11, 13, 15, 21, 25, 33, 35, 49]


def best_fit(n, odds):
    best = None
    for m in odds:
        j = 0
        v = m
        while v < n:
            v <<= 1
            j += 1
        if best is None or v < best[0]:
            best = (v, m, j)
    return best


rows = []
for line in err.splitlines():
    mm = re.match(r"\[ceil\]\s+(\w+)\s+need=(\d+)\s+used=(\d+)\s+cnt=(\d+)", line)
    if mm:
        rows.append((mm.group(1), int(mm.group(2)), int(mm.group(3)), int(mm.group(4))))

if not rows:
    # 尝试 CSV 风格
    for line in err.splitlines():
        parts = line.strip().split(",")
        if len(parts) == 4 and parts[1].isdigit():
            rows.append((parts[0], int(parts[1]), int(parts[2]), int(parts[3])))

if not rows:
    print("no CEILHIST rows parsed; raw stderr tail:")
    print(err[-2000:])
    sys.exit(0)

agg = collections.defaultdict(int)
for tag, need, used, cnt in rows:
    agg[(tag, need, used)] += cnt

tot = 0.0
items = []
for (tag, need, used), cnt in agg.items():
    w = used * math.log2(used) * cnt
    tot += w
    items.append((w, tag, need, used, cnt))
items.sort(reverse=True)

print("CASE=%s BIN=%s  total used*NlogN=%.4e" % (CASE, BIN, tot))
print("%-6s %9s %9s %5s %7s %8s | %-22s %-22s"
      % ("tag", "need", "used", "cnt", "share", "waste", "best{3,5,7,9,..}", "best{3,7} only"))
gain_all = 0.0
gain_37 = 0.0
for w, tag, need, used, cnt in items[:18]:
    share = 100.0 * w / tot
    waste = 100.0 * (used / need - 1)
    v_all, m_all, j_all = best_fit(need, ODDS)
    v_37, m_37, j_37 = best_fit(need, [1, 3, 7, 21])
    w_all = 100.0 * (v_all / need - 1)
    w_37 = 100.0 * (v_37 / need - 1)
    nw_all = v_all * math.log2(v_all) * cnt
    nw_37 = v_37 * math.log2(v_37) * cnt
    gain_all += (w - nw_all)
    gain_37 += (w - nw_37)
    print("%-6s %9d %9d %5d %6.2f%% %+7.1f%% | %2d*2^%-2d=%-8d %+5.1f%% | %2d*2^%-2d=%-8d %+5.1f%%"
          % (tag, need, used, cnt, share, waste,
             m_all, j_all, v_all, w_all, m_37, j_37, v_37, w_37))

print()
print("potential work reduction (top18):  full odd set = %.2f%%   {3,7} only = %.2f%%"
      % (100.0 * gain_all / tot, 100.0 * gain_37 / tot))
