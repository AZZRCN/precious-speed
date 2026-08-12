# AZZRCN
# https://github.com/AZZRCN
# v18 = v16 + 仅两个 O(m) 修复 (2 幂路径/代码布局保持 v16 原样).
# 验证: 修复能否在保留 v16 的 1.8% headline 优势的同时, 消除混合路径 O(m) 开销.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"; B = f"{H}/build"; S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
vmctl.run(f"mkdir -p {S} {B}")
vmctl.put(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "work", "mul", "v18.cpp"), f"{S}/v18.cpp")
rc, out, _ = vmctl.run(f"cd {S} && {CXX} v18.cpp -o {B}/v18 2>&1 | head -30; echo RC=${{PIPESTATUS[0]}}")
print(out)
if "RC=0" not in (out or ""):
    sys.exit(1)
# 正确性闸门
vmctl.run(f"cd {H} && bad=0; for f in data/mul/*.in; do e=${{f%.in}}.exp; [ -f \"$e\" ] || continue; "
          f"if ! build/v18 < $f | cmp -s - $e; then echo \"BAD $(basename $f)\"; bad=$((bad+1)); fi; done; "
          f"echo \"v18 gate bad=$bad\"")
# bigmix: v16 vs v18 (混合路径改进)
print("\n########## bigmix: v16(A) vs v18(B) ##########")
vmctl.run(f"cd {H} && python3 perfab.py data/_big {B}/v16 {B}/v18 -r 10", timeout=3600)
# headline 全量: v13w(A) vs v16(B) vs v18(C) vs wbmul
print("\n########## headline: v13w / v16 / v18 / wbmul ##########")
vmctl.run(f"cd {H} && python3 perfab.py data/mul {B}/v13w {B}/v16 {B}/v18 {B}/wbmul -r 10", timeout=7200)
