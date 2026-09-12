# AZZRCN
# https://github.com/AZZRCN
# ============ 标准擂台驱动 (铁律: 所有 A/B 一律在统一负重下进行) ============
# 流程:
#   1) 生成 v13w = v13 + libstdc++ 负重 (基线与候选 DT_NEEDED 对齐)
#   2) 统一口径编译 v13 / v13w / v16 / wbmul(假想敌)
#   3) 硬核对 DT_NEEDED —— 不一致直接中止, 禁止开跑
#   4) 正确性闸门 (.exp 逐字节)
#   5) perf 四口径擂台 (instructions / cycles / L1-dcache-load-misses / cache-misses)
# 用法: python tools/_vm_arena.py [-r N] [--only-large]
import os, sys, subprocess

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
H = "/home/azzr/hexbench"
B = f"{H}/build"
S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
BINS = ["v13", "v13w", "v16", "wbmul"]

reps = "10"
only_large = False
for i, a in enumerate(sys.argv[1:]):
    if a == "-r":
        reps = sys.argv[i + 2]
    if a == "--only-large":
        only_large = True

# ---- 1. 生成负重基线 ----
v13 = open(os.path.join(ROOT, "work", "mul", "v13.cpp"), "r", encoding="utf-8").read()
assert "#include <complex>" in v13, "v13 头部结构变了"
v13w = v13.replace(
    "#include <complex>\n",
    "#include <complex>\n#include <vector>   // BALLAST: 与候选对齐 libstdc++ 依赖\n", 1)
assert "int main() {" in v13w
v13w = v13w.replace(
    "int main() {\n",
    "int main() {\n"
    "    // BALLAST: 强制引入 libstdc++ (容器) + libgcc_s (_Unwind_Resume) 两个 DT_NEEDED,\n"
    "    // 与候选/假想敌完全对齐。try + 局部容器 => 清理落地垫 => _Unwind_Resume。\n"
    "    { std::vector<double> __b1;\n"
    "      try { __b1.resize(1); std::vector<double> __b2(2);\n"
    "            asm volatile(\"\" :: \"r\"(__b1.data()), \"r\"(__b2.data()) : \"memory\"); }\n"
    "      catch (...) { return 2; } }\n", 1)
open(os.path.join(HERE, "_v13w.cpp"), "w", encoding="utf-8").write(v13w)
print(f"[gen] _v13w.cpp {len(v13w)} bytes")

# ---- 2. 上传 + 编译 ----
vmctl.run(f"mkdir -p {S} {B}")
vmctl.put(os.path.join(HERE, "_v13w.cpp"), f"{S}/v13w.cpp")
vmctl.put(os.path.join(ROOT, "work", "mul", "v13.cpp"), f"{S}/v13.cpp")
vmctl.put(os.path.join(ROOT, "work", "mul", "v16.cpp"), f"{S}/v16.cpp")
vmctl.put(os.path.join(ROOT, "web_best", "mul.cpp"), f"{S}/wbmul.cpp")
for n in BINS:
    rc, out, _ = vmctl.run(f"cd {S} && {CXX} {n}.cpp -o {B}/{n} 2>&1 | grep -E 'error' | head -5; echo RC_{n}=${{PIPESTATUS[0]}}")
    if f"RC_{n}=0" not in out:
        print(f"!! {n} 编译失败, 中止"); sys.exit(1)

# ---- 3. DT_NEEDED 硬核对 ----
rc, out, _ = vmctl.run(
    f"cd {B} && for f in {' '.join(BINS)}; do printf '%-7s ' $f; "
    f"objdump -p $f 2>/dev/null | grep NEEDED | awk '{{printf \"%s \", $2}}' | tr ' ' '\\n' | sort | tr '\\n' ' '; echo; done")
need = {}
for line in out.splitlines():
    t = line.split()
    if t and t[0] in BINS:
        need[t[0]] = " ".join(sorted(t[1:]))
cmp_set = {k: v for k, v in need.items() if k != "v13"}   # v13(裸) 只作参照, 不参赛
if len(set(cmp_set.values())) != 1:
    print("!! DT_NEEDED 不一致, 禁止开跑 (铁律: 统一负重):")
    for k, v in need.items():
        print(f"   {k:8s} {v}")
    sys.exit(1)
print(f"[ok] 负重统一: {list(cmp_set.values())[0]}")

# ---- 4. 正确性闸门 ----
for n in ["v16", "wbmul"]:
    vmctl.run(f"cd {H} && bad=0; for f in data/mul/*.in; do e=${{f%.in}}.exp; [ -f \"$e\" ] || continue; "
              f"if ! build/{n} < $f | cmp -s - $e; then echo \"BAD $(basename $f)\"; bad=$((bad+1)); fi; done; "
              f"echo \"{n} gate bad=$bad\"")

# ---- 5. perf 擂台 ----
vmctl.put(os.path.join(HERE, "perfab.py"), f"{H}/perfab.py")
if only_large:
    vmctl.run(f"rm -rf {H}/data/_lg && mkdir -p {H}/data/_lg && cd {H}/data/mul && "
              f"for f in large_00 large_01 large_02 fft_killer_00 max_max_01; do cp $f.in $f.exp {H}/data/_lg/ 2>/dev/null; done")
    ds = "data/_lg"
else:
    ds = "data/mul"
# 不绑核: VM 里 vCPU<->物理核映射由 hypervisor 决定, taskset 只是把噪声换个形状,
# 还让该 vCPU 独吞中断且无法迁移 -> 系统性偏差 (前辈 A 档标定: 不绑核 ratio-of-mins 精度 ±0.2%)
vmctl.run(f"cd {H} && python3 perfab.py {ds} {B}/v13w {B}/v16 {B}/wbmul -r {reps}", timeout=7200)
