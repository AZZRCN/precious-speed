#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""完整形状 dump: 每个用例的**全部** query 的 absDivMu 形状 (不做 head 截断)。

动机: 之前 d16_check.py 用 `head -2` 只看前两条, 导致按用例做代价累加时漏算
      —— division_of_big_integers 是 **多 query** 输入, 一个 .in 里有 T 组除法。
      a_max_b_random_02 看似只有 1 次调用, 实际很可能有多组不同形状。
      不修正这个记账, 任何"模型 vs callgrind"的横向对比都是错的。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run  # noqa: E402

PS = r"D:\precious_speed"
CASES = ["length_ratio_integer_00", "length_ratio_integer_01",
         "length_ratio_integer_02", "length_ratio_integer_03",
         "a_max_b_random_02", "r_nearly_zero_01", "large_01", "max_00"]

sh = "cd ~/divbench\nIN=/home/azzr/lcp/big_integer/division_of_big_integers/in\n"
for c in CASES:
    sh += (f'echo "=== {c}"; ./bin/d16s < $IN/{c}.in >/dev/null 2>/tmp/sf.txt; '
           f'echo -n "  queries(T)="; head -1 $IN/{c}.in; '
           f'grep -c "\\[mushape\\]" /tmp/sf.txt | sed "s/^/  mu_calls=/"; '
           f'grep "\\[mushape\\]" /tmp/sf.txt | sort | uniq -c | sort -rn\n')

rc, out, err = run(sh, timeout=3600)
print(out)
print(err[-1500:])
with open(os.path.join(PS, "lc_bench", "vm", "_shape_full.log"), "w",
          encoding="utf-8") as f:
    f.write(out)
