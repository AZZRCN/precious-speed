#!/usr/bin/env python3
# 找 burnikel_ziegler_bound seed=2 中首个 CYCLIC=1 错例 (且 len2>64 确保走 absDivMu)
import subprocess, os
GEN, SEED = "burnikel_ziegler_bound", 2
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
print(f"T={T}")
for idx, (A, B) in enumerate(cases):
    if len(B) <= 64 * 4:   # len2 <= 64 -> schoolbook, skip
        continue
    a = int(A, 16); b = int(B, 16); tq = a // b; tr = a % b
    env = dict(os.environ); env["CYCLIC"] = "1"
    r = subprocess.run(["./div_base16_dbg"], input="1\n" + A + " " + B + "\n",
                       capture_output=True, text=True, timeout=120, env=env)
    toks = r.stdout.split()
    if len(toks) < 2:
        print(f"CASE={idx+1} CRASH/empty out"); continue
    q, rr = toks[0], toks[1]
    q_ok = int(q, 16) == tq
    r_ok = int(rr, 16) == tr
    if not q_ok or not r_ok:
        print(f"FIRST_BAD_CASE={idx+1} lenA_hex={len(A)} lenB_hex={len(B)} q_ok={q_ok} r_ok={r_ok}")
        break
else:
    print("NO_BAD_CASE_FOUND (all CYCLIC=1 matched oracle among len2>64)")
