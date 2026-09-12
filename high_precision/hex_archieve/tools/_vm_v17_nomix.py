# AZZRCN
# https://github.com/AZZRCN
# 归因隔离: v17nomix = v17 -DV16_NOMIX (混合档关闭, 代码仍在)
#   v13w vs v17nomix  -> 差异 = 纯"代码膨胀/布局"代价 (headline 点本就不走混合)
#   v17nomix vs v17   -> 差异 = 混合路径的净收益
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
B = f"{H}/build"; S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
rc, out, _ = vmctl.run(f"cd {S} && {CXX} -DV16_NOMIX v17.cpp -o {B}/v17nm 2>&1 | head -20; echo RC=${{PIPESTATUS[0]}}")
print(out)
if "RC=0" not in (out or ""):
    sys.exit(1)
# 子集: headline 族 (max_max/fft_killer) + 混合受益族 (large/large_small)
vmctl.run(f"rm -rf {H}/data/_iso && mkdir -p {H}/data/_iso && cd {H}/data/mul && "
          f"for f in max_max_00 max_max_04 max_max_06 fft_killer_00 fft_killer_01 "
          f"large_00 large_01 large_02 large_small_00; do cp $f.in $f.exp {H}/data/_iso/; done")
vmctl.run(f"cd {H} && python3 perfab.py data/_iso {B}/v13w {B}/v17nm {B}/v17 -r 10", timeout=7200)
