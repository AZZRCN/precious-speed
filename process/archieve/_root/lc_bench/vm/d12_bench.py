#!/usr/bin/env python3
"""D12 (radix-3 FFT 档位) vs D11 (SWAR) vs D10 配对比值测速。

纪律: 预热丢弃 + 交替运行 + rep 轮转 + median-of-ratios (见 divratio.py)。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
REPS = sys.argv[1] if len(sys.argv) > 1 else "9"

for name in ("div_D10", "div_D11", "div_D12"):
    src = os.path.join(PS, "best", f"{name}.cpp")
    if os.path.exists(src):
        put(src, f"/home/azzr/divbench/src/{name}.cpp")

run("cd ~/divbench/src && for n in div_D10 div_D11 div_D12; do "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/$n $n.cpp || echo BUILDFAIL_$n; done; echo done",
    timeout=1200)
run(f"cd ~/divbench && python3 ~/divratio.py {REPS} div_D11 div_D12 div_D10", timeout=3600)
