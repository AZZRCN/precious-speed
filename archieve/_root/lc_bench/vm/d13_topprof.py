#!/usr/bin/env python3
"""D13 顶层四段计时: read / parse / div / fmt / write。

回答: D12(57ms) 之后, length_ratio_integer_03 与 r_nearly_zero_01 这类
"FFT 工作量很小却依然很慢" 的用例, 时间到底花在哪里。
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
    "a_max_b_random_02", "r_nearly_zero_01",
    "burnikel_ziegler_bound_01", "large_00",
]

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")
rc, out, err = run(
    "cd ~/divbench/src && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DTOPPROF -o ../bin/tp_d13 div_D13.cpp "
    "2>&1 | tail -20 && echo BUILD_DONE", timeout=1200)
print(out)
if "BUILD_DONE" not in out:
    sys.exit("build failed")

# 每例跑 3 次取最后一次(预热后)
cmd = ["cd ~/divbench"]
for c in CASES:
    cmd.append(
        f'echo "### {c}"; '
        f'for i in 1 2 3; do ./bin/tp_d13 < {IN}/{c}.in > /dev/null 2>/tmp/tp.txt; done; '
        f'grep topprof /tmp/tp.txt')
rc, out, err = run("; ".join(cmd), timeout=1800)
print(out)
