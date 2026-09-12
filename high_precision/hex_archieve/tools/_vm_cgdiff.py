# AZZRCN
# https://github.com/AZZRCN
# 精确定位 v17nm 相对 v13w 多出的 770,209 条指令 (cachegrind --diff, 确定性)。
# -g 只加调试信息, 不改 -O2 代码生成。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"; B = f"{H}/build"; S = f"{H}/scratch"
CXX = "g++ -O2 -g -march=x86-64-v3 -std=c++23"
CASE = "data/mul/max_max_00.in"
vmctl.run(f"cd {S} && {CXX} v13w.cpp -o {B}/v13wg && {CXX} -DV16_NOMIX v17.cpp -o {B}/v17nmg && echo BUILD_OK")
for b in ["v13wg", "v17nmg"]:
    vmctl.run(f"cd {H} && rm -f /tmp/cgd_{b}.out && valgrind --tool=cachegrind --cache-sim=no "
              f"--cachegrind-out-file=/tmp/cgd_{b}.out {B}/{b} < {CASE} > /dev/null 2>/dev/null; echo done_{b}")
vmctl.run("cg_annotate --diff --no-annotate --threshold=0.3 /tmp/cgd_v13wg.out /tmp/cgd_v17nmg.out 2>&1 "
          "| sed -n '/Function:file summary/,/Metadata/p' | head -50")
