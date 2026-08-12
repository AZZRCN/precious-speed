# AZZRCN
# https://github.com/AZZRCN
# 编译 v16 的隔离自测 (schoolbook 参照), 分路径统计正确性。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

SRC = "/home/azzr/hexbench/src"
LOCAL = r"d:\hex_precious_speed"

vmctl.put(os.path.join(LOCAL, "work/mul/v16.cpp"), f"{SRC}/v16.cpp")
rc, _, _ = vmctl.run(
    f"cd {SRC} && g++ -O2 -march=x86-64-v3 -std=c++23 -DV16_SELFTEST -o /tmp/v16st v16.cpp 2>&1 | grep -v Wignored-attributes | grep -v 'std::vector<cpx>' | grep -v '\\^' ; echo BUILD_DONE && /tmp/v16st"
)
