#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""编译 div_D12 (LC 冠军 #390291, 57ms) 并测 Zen3 est —— 建立 est <-> LC ms 锚定。

D12 的 LC 逐点回执 (已知 ground truth):
    length_ratio_integer_00 = 55 ms
    length_ratio_integer_01 = 56 ms
    length_ratio_integer_02 = 57 ms
    length_ratio_integer_03 = 57 ms
    r_nearly_zero_01        = 51 ms
    a_max_b_random_02       = 53 ms

有了 (est, ms) 6 个点就能线性拟合 ms = alpha*est + beta,
其中 beta 吸收 I/O + 进程启动等固定开销。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(os.path.dirname(HERE)), "best", "div_D12.cpp")
if not os.path.exists(SRC):
    SRC = r"D:\precious_speed\best\div_D12.cpp"

print("uploading", SRC)
put(SRC, "/home/azzr/divbench/src/div_D12.cpp")

rc, out, err = run(
    "cd /home/azzr/divbench && mkdir -p bin && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d12 src/div_D12.cpp 2>&1 | tail -20 && "
    "ls -la bin/d12",
    timeout=600,
)
print(out)
print(err)
