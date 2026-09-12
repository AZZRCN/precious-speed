#!/usr/bin/env python3
"""上传 div_D12.cpp + fft3_test.cpp 到 VM，编译并运行 radix-3 自检。"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))

put(os.path.join(PS, "best", "div_D12.cpp"), "/home/azzr/divbench/src/div_D12.cpp")
put(os.path.join(HERE, "fft3_test.cpp"), "/home/azzr/divbench/src/fft3_test.cpp")

run("cd ~/divbench/src && g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/fft3_test "
    "fft3_test.cpp 2>&1 | head -40 ; echo BUILD_RC=$?")
run("cd ~/divbench && ./bin/fft3_test")
