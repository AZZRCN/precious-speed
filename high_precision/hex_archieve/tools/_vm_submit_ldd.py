# AZZRCN
# https://github.com/AZZRCN
# 检查最终提交文件 submit_ready/mul.cpp 是否链接 libstdc++ (白送 1.63M 指令启动开销)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
HERE = os.path.dirname(os.path.abspath(__file__))
vmctl.put(os.path.join(HERE, "..", "submit_ready", "mul.cpp"), f"{H}/scratch/submul.cpp")
vmctl.run(f"cd {H}/scratch && g++ -O2 -march=x86-64-v3 -std=c++23 submul.cpp -o {H}/build/submul 2>&1 | head -5; echo BUILD_DONE")
vmctl.run(f"cd {H}/build && for f in v13 submul; do echo \"--- $f ---\"; ldd $f 2>&1 | head -6; done")
vmctl.run(f"cd {H} && for f in v13 submul; do echo \"--- $f ---\"; perf stat -e instructions:u -x, build/$f < data/mul/example_00.in > /dev/null; done")
