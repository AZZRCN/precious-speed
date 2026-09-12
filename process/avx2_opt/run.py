"""avx2_opt 总驱动：生成输入 → 在 .66 VM 上跑优化器闭环 → 打印报告。

演示两个目标：
  (A) examples/hex_add/naive.cpp  —— 朴素标量 I/O，优化器应自动套用 SIMD 变换并测得 Ir 下降
  (B) hex_best/add_opt.cpp         —— 已优化版，优化器应检测"无可用变换"→ 报告已触底（地板）
"""
import os
import sys
import random

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from avx2_opt import harness, remote

HERE = os.path.dirname(os.path.abspath(__file__))
NAIVE = os.path.join(HERE, "examples", "hex_add", "naive.cpp")
REAL = r"D:\precious_speed\hex_best\add_opt.cpp"
SAMPLE = os.path.join(HERE, "examples", "hex_add", "sample.in")


def gen_input(path, T=60000, seed=1234):
    rng = random.Random(seed)
    with open(path, "w") as f:
        f.write(f"{T}\n")
        for _ in range(T):
            la = rng.randint(1, 16)
            lb = rng.randint(1, 16)
            a = rng.randrange(16 ** la)
            b = rng.randrange(16 ** lb)
            f.write(f"{a:x} {b:x}\n")


def main():
    ok, out, err = remote.check()
    print(f"[连通性] reachable={ok}")
    if not ok:
        print(out, err)
        sys.exit(1)

    gen_input(SAMPLE)
    print(f"[输入] 已生成 {SAMPLE} (T=60000)")

    print("\n========== (A) 朴素 HEX ADD —— 优化器自动套用 SIMD ==========")
    r1 = harness.run_task(NAIVE, SAMPLE, name="naive")
    print(f"ref_total_ir = {r1.get('ref_total_ir'):,}")
    print("ref hotspot:\n" + r1.get("ref_hotspot", ""))
    print("detected transforms:", r1.get("detected"))
    for s in r1.get("steps", []):
        print("  step:", s)

    print("\n========== (B) 已优化 HEX ADD —— 是否触底 ==========")
    r2 = harness.run_task(REAL, SAMPLE, name="real", only_detect=False)
    print(f"ref_total_ir = {r2.get('ref_total_ir'):,}")
    print("ref hotspot:\n" + r2.get("ref_hotspot", ""))
    print("detected transforms:", r2.get("detected"))
    for s in r2.get("steps", []):
        print("  step:", s)

    print("\n[完成]")


if __name__ == "__main__":
    main()
