# AZZRCN
# https://github.com/AZZRCN
# 定位 v17 在 2 幂 headline 上相对 v13w 多出的 0.56% (~770K) 指令来自哪个符号。
# 用 perf record -e instructions:u (架构量, 采样但分布可靠), 两边同 case 同次数。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
B = f"{H}/build"
CASE = "data/mul/max_max_00.in"
for b in ["v13w", "v17nm"]:
    vmctl.run(f"cd {H} && rm -f /tmp/p_{b}.data && "
              f"perf record -q -e instructions:u -c 200000 -o /tmp/p_{b}.data "
              f"{B}/{b} < {CASE} > /dev/null 2>/dev/null; "
              f"perf report -i /tmp/p_{b}.data --stdio --no-children -F overhead,symbol 2>/dev/null "
              f"| grep -v '^#' | grep -v '^$' | head -25")
