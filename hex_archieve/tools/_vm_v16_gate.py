# AZZRCN
# 驱动: 上传 v13/v16 + bench_ab.py 到 VM, 用标准口径编译, 跑正确性闸门(.exp 逐字节 md5) + A/B.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

SRC = "/home/azzr/hexbench/src"
BUILD = "/home/azzr/hexbench/build"
DATA = "/home/azzr/hexbench/data/mul"
LOCAL = r"d:\hex_precious_speed"

FLAGS = "-O2 -march=x86-64-v3 -std=c++23"

# 1) 上传
vmctl.put(os.path.join(LOCAL, "work/mul/v13.cpp"), f"{SRC}/v13.cpp")
vmctl.put(os.path.join(LOCAL, "work/mul/v16.cpp"), f"{SRC}/v16.cpp")
vmctl.put(os.path.join(LOCAL, "tools/bench_ab.py"), f"{BUILD}/../bench_ab.py")

# 2) 编译 (标准口径)
vmctl.run(f"mkdir -p {SRC} {BUILD}")
rc, _, _ = vmctl.run(f"cd {SRC} && g++ {FLAGS} -o {BUILD}/v13 v13.cpp && echo BUILD_V13_OK")
assert rc == 0 and os.path.exists  # noop guard
rc, _, _ = vmctl.run(f"cd {SRC} && g++ {FLAGS} -o {BUILD}/v16 v16.cpp && echo BUILD_V16_OK")
assert rc == 0

# 3) 正确性闸门 + 首轮 A/B (K=3, cpu=2). A=v16, B=v13.
#    bench_ab 对每个 case 分别把 A/B 的输出 md5 与 .exp 比对 -> 全部 OK 即正确性过关.
vmctl.run(f"cd {BUILD}/.. && python3 bench_ab.py {BUILD}/v16 {BUILD}/v13 3 2")
