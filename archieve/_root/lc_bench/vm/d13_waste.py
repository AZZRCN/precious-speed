#!/usr/bin/env python3
"""D13: 统计各 LC 用例的 "FFT 需求长度 -> 实际档位" 浪费分布。

用于定位 D12(57ms) 之后的新瓶颈: length_ratio_integer_00..03 是否仍被档位粒度卡住,
以及加 5*2^k / 7*2^k 档位能吃到多少。
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
    "a_max_b_random_02", "r_nearly_zero_01",
]

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")
rc, out, err = run(
    "cd ~/divbench/src && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DCEILHIST -o ../bin/w_d13 div_D13.cpp "
    "2>&1 | tail -20 && echo BUILD_DONE", timeout=1200)
print(out)
if "BUILD_DONE" not in out:
    sys.exit("build failed")

for c in CASES:
    print(f"\n########## {c} ##########")
    rc, out, err = run(
        f"cd ~/divbench && ./bin/w_d13 < {IN}/{c}.in > /dev/null 2>/tmp/w.txt; "
        f"cat /tmp/w.txt", timeout=600)
    print(out.strip())
