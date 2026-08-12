# AZZRCN
# https://github.com/AZZRCN
# 归因对照: v16 关掉混合档(-DV16_NOMIX) vs v13。
# 若 ratio ~ 1.00 -> 退化全部来自混合路径本身; 若仍 <1 -> 代码膨胀/布局问题。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

SRC = "/home/azzr/hexbench/src"
BUILD = "/home/azzr/hexbench/build"
LOCAL = r"d:\hex_precious_speed"

vmctl.put(os.path.join(LOCAL, "work/mul/v16.cpp"), f"{SRC}/v16.cpp")
rc, _, _ = vmctl.run(
    f"cd {SRC} && g++ -O2 -march=x86-64-v3 -std=c++23 -DV16_NOMIX -o {BUILD}/v16nm v16.cpp 2>/dev/null && echo BUILD_OK")
assert rc == 0, "build failed"
vmctl.run(f"cd {BUILD}/.. && python3 bench_ab.py {BUILD}/v16nm {BUILD}/v13 9 2")
