# AZZRCN
# https://github.com/AZZRCN
# Q1 Step 0: cachegrind 比对 D refs (Dr+Dw). --cache-sim=yes 必须显式 (Valgrind 3.23+ 默认 no)
# Zen3 几何硬编码 (VM lscpu 拓扑是假的):
#   --I1=32768,8,64 --D1=32768,8,64
#   --LL=524288,8,64   (L2 视角, 512K/8-way)
#   --LL=33554432,16,64 (L3 视角, 32M/16-way 单 CCX)
# 注意: set = size/(assoc*line) 必须是 2 的幂; 36M/16-way 非 2 幂 -> 改 18-way (sets=32768)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
def cg(binname, case, ll):
    out = f"/tmp/cg_{binname}_{case}_{ 'L2' if ll==524288 else 'L3' }.out"
    cmd = (f"cd {H} && rm -f {out} && valgrind --tool=cachegrind --cache-sim=yes "
           f"--I1=32768,8,64 --D1=32768,8,64 --LL={ll},{18 if ll>524288 else 8},64 "
           f"--cachegrind-out-file={out} build/{binname} < data/_big/{case}.in > /dev/null 2>&1; "
           f"cg_annotate --auto=no {out} 2>/dev/null | grep -E '^I1| D refs| I refs| D1  miss| LLi| LLd miss' | head")
    return vmctl.run(cmd, timeout=900)
for case in ["pow2_big", "mix3_big"]:
    for ll in [524288, 33554432]:
        print(f"===== {case}  LL={ll} =====")
        print(cg("v13w", case, ll))
        print(cg("v16", case, ll))
