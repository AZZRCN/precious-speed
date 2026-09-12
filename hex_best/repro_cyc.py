#!/usr/bin/env python3
# 决定性探针: 对失败 case 分别跑 CYCLIC=1 与 NOCYCLIC=1, 逐块 diff CYCBLK dump.
# linear (NOCYCLIC) 是正确真值, 第一次分叉的块 = bug 位置.
import subprocess, os, sys

GEN = sys.argv[1] if len(sys.argv) > 1 else "burnikel_ziegler_bound"
SEED = int(sys.argv[2]) if len(sys.argv) > 2 else 2
CASE = int(sys.argv[3]) if len(sys.argv) > 3 else 512

out = subprocess.run(["./genbin/" + GEN, str(SEED)], capture_output=True, text=True).stdout
lines = out.split("\n")
T = int(lines[0])
cases = []
i = 1
for _ in range(T):
    if i >= len(lines):
        break
    p = lines[i].split()
    if len(p) < 2:
        i += 1
        continue
    cases.append((p[0], p[1]))
    i += 1
A, B = cases[CASE - 1]
print(f"GEN={GEN} SEED={SEED} CASE={CASE} lenA={len(A)} lenB={len(B)}")

def run(env_extra):
    env = dict(os.environ)
    env["CYCLIC_DEBUG"] = "1"
    env.update(env_extra)
    r = subprocess.run(["./div_base16_dbg"], input="1\n" + A + " " + B + "\n",
                       capture_output=True, text=True, timeout=120, env=env)
    return r

r1 = run({"CYCLIC": "1"})      # cyclic path
r2 = run({"NOCYCLIC": "1"})    # linear path (ground truth)
cb1 = [l for l in r1.stderr.split("\n") if l.startswith("CYCBLK")]
cb2 = [l for l in r2.stderr.split("\n") if l.startswith("CYCBLK")]
print("=== CYCLIC stdout (q,r) ===", r1.stdout.split())
print("=== LINEAR stdout (q,r) ===", r2.stdout.split())
print(f"=== blocks: cyclic={len(cb1)} linear={len(cb2)} ===")
first_diff = -1
for i in range(max(len(cb1), len(cb2))):
    a = cb1[i] if i < len(cb1) else "<none>"
    b = cb2[i] if i < len(cb2) else "<none>"
    if a != b:
        if first_diff < 0:
            first_diff = i
        print(f"[BLOCK {i}] DIFF")
        print("  CYC:", a)
        print("  LIN:", b)
    else:
        print(f"[BLOCK {i}] SAME")
print(f"=== FIRST DIFF BLOCK = {first_diff} ===")
