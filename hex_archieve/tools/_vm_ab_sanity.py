# AZZRCN
# https://github.com/AZZRCN
# harness 自检: (1) v13 vs v13 自比  (2) v13 vs v16nm 反向位置
# 若 (1) 明显偏离 1.00 或 (2) 出现 ~1/0.953, 则 bench_ab 存在 A/B 位置偏差。
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
B = "/home/azzr/hexbench/build"
vmctl.run(f"cp {B}/v13 {B}/v13copy && chmod +x {B}/v13copy")
print("===== (1) v13copy(A) vs v13(B)  自比 =====")
vmctl.run(f"cd {B}/.. && python3 bench_ab.py {B}/v13copy {B}/v13 9 2")
print("===== (2) v13(A) vs v16nm(B)  反向 =====")
vmctl.run(f"cd {B}/.. && python3 bench_ab.py {B}/v13 {B}/v16nm 9 2")
