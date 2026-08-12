# AZZRCN
# https://github.com/AZZRCN
# 统一负重 A/B 台。
#   动机: v16 因 std::vector 链了 libstdc++/libgcc_s, 白背 ~1.63M 指令动态链接开销;
#         v13 只链 libm/libc。直接 A/B 等于拿轻装比重装, 结论不可信。
#   做法: 给 v13 注入等价负重 (v13w), 让双方 DT_NEEDED 一致, 差值即纯算法差异。
#   同时: web_best/mul.cpp (真·假想敌, LC #1 快照) 天然带 libstdc++, 一并上擂台。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
H = "/home/azzr/hexbench"
B = f"{H}/build"
S = f"{H}/scratch"

# ---- 1. 生成 v13w.cpp = v13.cpp + libstdc++ 负重 ----
v13 = open(os.path.join(ROOT, "work", "mul", "v13.cpp"), "r", encoding="utf-8").read()
assert "#include <complex>" in v13, "v13 头部结构变了"
BALLAST_INC = "#include <complex>\n#include <vector>   // BALLAST: 与 v16 对齐 libstdc++ 依赖\n"
v13w = v13.replace("#include <complex>\n", BALLAST_INC, 1)
assert "int main() {" in v13w
BALLAST_BODY = (
    "int main() {\n"
    "    // BALLAST: 强制引入 libstdc++ 符号, 使 DT_NEEDED 与 v16 一致 (公平 A/B)\n"
    "    { static std::vector<double> __bal; __bal.resize(1);\n"
    "      asm volatile(\"\" :: \"r\"(__bal.data()) : \"memory\"); }\n"
)
v13w = v13w.replace("int main() {\n", BALLAST_BODY, 1)
open(os.path.join(HERE, "_v13w.cpp"), "w", encoding="utf-8").write(v13w)
print(f"[gen] _v13w.cpp  {len(v13w)} bytes")

# ---- 2. 上传 ----
vmctl.run(f"mkdir -p {S} {B}")
vmctl.put(os.path.join(HERE, "_v13w.cpp"), f"{S}/v13w.cpp")
vmctl.put(os.path.join(ROOT, "work", "mul", "v13.cpp"), f"{S}/v13.cpp")
vmctl.put(os.path.join(ROOT, "work", "mul", "v16.cpp"), f"{S}/v16.cpp")
vmctl.put(os.path.join(ROOT, "web_best", "mul.cpp"), f"{S}/wbmul.cpp")

# ---- 3. 编译 (统一口径) ----
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
for name in ["v13", "v13w", "v16", "wbmul"]:
    vmctl.run(f"cd {S} && {CXX} {name}.cpp -o {B}/{name} 2>&1 | grep -vE 'warning|note|^ |^\\s*\\^|~~~' | head -5; echo BUILD_{name}_rc=$?")

# ---- 4. DT_NEEDED 核对 ----
vmctl.run(f"cd {B} && for f in v13 v13w v16 wbmul; do printf '%-7s ' $f; "
          f"objdump -p $f 2>/dev/null | grep NEEDED | awk '{{printf \"%s \", $2}}'; echo; done")

# ---- 5. 空载启动指令数 (example_00 极小输入, 主要反映固定开销) ----
vmctl.run(f"cd {H} && for f in v13 v13w v16 wbmul; do printf '%-7s ' $f; "
          f"perf stat -e instructions:u -x, build/$f < data/mul/example_00.in > /dev/null 2>/tmp/e_$f; "
          f"head -1 /tmp/e_$f; done")
