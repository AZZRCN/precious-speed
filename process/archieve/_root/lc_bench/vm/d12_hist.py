#!/usr/bin/env python3
"""对 max_* / large_* 等用例, 比较 D12(fft3) 与 D12(NO_FFT3) 的 FFT 长度直方图。

同一份源码 A/B, 排除除档位以外的一切差异。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
CASES = sys.argv[1:] or ["max_02", "max_00", "large_01", "a_max_b_random_02",
                         "length_ratio_integer_02"]

put(os.path.join(PS, "best", "div_D12.cpp"), "/home/azzr/divbench/src/div_D12.cpp")
run("cd ~/divbench/src && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DFFTHIST -o ../bin/h_fft3 div_D12.cpp && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DFFTHIST -DNO_FFT3 -o ../bin/h_2pow div_D12.cpp && "
    "echo BUILD_OK", timeout=900)

for c in CASES:
    print(f"\n########## {c} ##########")
    run(f"cd ~/divbench && for v in h_2pow h_fft3; do echo \"--- $v ---\"; "
        f"./bin/$v < /home/azzr/lcp/big_integer/division_of_big_integers/in/{c}.in "
        f"> /dev/null 2>/tmp/h_$v.txt; tail -25 /tmp/h_$v.txt; done", timeout=900)
