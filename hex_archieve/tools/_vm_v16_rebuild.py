# AZZRCN
# 重新上传 v16.cpp, 仅重建 v16, 跑正确性闸门(.exp) + A/B(v16 vs v13).
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

SRC = "/home/azzr/hexbench/src"
BUILD = "/home/azzr/hexbench/build"
LOCAL = r"d:\hex_precious_speed"

vmctl.put(os.path.join(LOCAL, "work/mul/v16.cpp"), f"{SRC}/v16.cpp")
rc, _, _ = vmctl.run(f"cd {SRC} && g++ -O2 -march=x86-64-v3 -std=c++23 -o {BUILD}/v16 v16.cpp && echo BUILD_V16_OK")
assert rc == 0, "v16 build failed"
vmctl.run(f"cd {BUILD}/.. && python3 bench_ab.py {BUILD}/v16 {BUILD}/v13 5 2")
