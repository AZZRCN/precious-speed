# AZZRCN
# https://github.com/AZZRCN
# v17 验证: 修掉 v16 混合路径的两个 O(m) 实现 bug 后重测
#   fix1: MR_Tw 顶层旋转因子 O(m) sin/cos 平表 -> O(sqrt(m)) 两级小表 (__sincos_fma 18.67%)
#   fix2: MR_Q 位反转表 O(m*bits) 双重循环 -> O(m) 递推        (MR_Q::get   10.67%)
# 流程: 上传编译 -> 22 点 .exp 正确性闸门 -> bigmix 三点 perfab (v16 vs v17 vs v13w)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
H = "/home/azzr/hexbench"
B = f"{H}/build"
S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"

vmctl.run(f"mkdir -p {S} {B}")
vmctl.put(os.path.join(ROOT, "work", "mul", "v17.cpp"), f"{S}/v17.cpp")
rc, out, err = vmctl.run(f"cd {S} && {CXX} v17.cpp -o {B}/v17 2>&1 | head -40; echo RC=${{PIPESTATUS[0]}}")
print(out)
if "RC=0" not in (out or ""):
    print("!! v17 编译失败, 中止"); sys.exit(1)

# DT_NEEDED 核对 (v17 必须与 v16/v13w 一致)
vmctl.run(f"cd {B} && for f in v13w v16 v17; do printf '%-6s ' $f; "
          f"objdump -p $f 2>/dev/null | grep NEEDED | awk '{{printf \"%s \", $2}}'; echo; done")

# 正确性闸门: 22 点 .exp 逐字节
vmctl.run(f"cd {H} && bad=0; for f in data/mul/*.in; do e=${{f%.in}}.exp; [ -f \"$e\" ] || continue; "
          f"if ! build/v17 < $f | cmp -s - $e; then echo \"BAD $(basename $f)\"; bad=$((bad+1)); fi; done; "
          f"echo \"v17 gate bad=$bad\"")
# 大输入闸门
vmctl.run(f"cd {H} && bad=0; for f in data/_big/*.in; do e=${{f%.in}}.exp; [ -f \"$e\" ] || continue; "
          f"if ! build/v17 < $f | cmp -s - $e; then echo \"BAD $(basename $f)\"; bad=$((bad+1)); fi; done; "
          f"echo \"v17 big gate bad=$bad\"")

# bigmix 擂台: 先看 v16 -> v17 的改进幅度
print("\n########## bigmix: v16(A) vs v17(B) ##########")
vmctl.run(f"cd {H} && python3 perfab.py data/_big {B}/v16 {B}/v17 -r 10", timeout=3600)
print("\n########## bigmix: v13w(A) vs v17(B) ##########")
vmctl.run(f"cd {H} && python3 perfab.py data/_big {B}/v13w {B}/v17 -r 10", timeout=3600)
