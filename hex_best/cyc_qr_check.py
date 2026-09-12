#!/usr/bin/env python3
# 实测 cyclic 到底错在 q 还是 r (Python oracle 真值).
# 用法: cyc_qr_check.py <GEN> <SEED> [divbin]
import subprocess, os, sys

GEN = sys.argv[1] if len(sys.argv) > 1 else "burnikel_ziegler_bound"
SEED = int(sys.argv[2]) if len(sys.argv) > 2 else 2
divbin = sys.argv[3] if len(sys.argv) > 3 else "./div_base16_zn"

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

def run_cyc(A, B):
    env = dict(os.environ)
    env["CYCLIC"] = "1"
    env.pop("NOCYCLIC", None)
    r = subprocess.run([divbin], input="1\n" + A + " " + B + "\n",
                       capture_output=True, text=True, timeout=300, env=env)
    return r.stdout.split()

q_wrong_r_right = 0
q_right_r_wrong = 0
both_wrong = 0
both_right = 0
samples = []
for (A, B) in cases:
    a = int(A, 16); b = int(B, 16)
    tq = a // b; tr = a % b
    c = run_cyc(A, B)
    if len(c) < 2:
        both_wrong += 1
        if len(samples) < 6:
            samples.append(("PARSE", A, B, c, [hex(tq), hex(tr)]))
        continue
    gq = int(c[0], 16); gr = int(c[1], 16)
    q_ok = (gq == tq); r_ok = (gr == tr)
    if q_ok and r_ok:
        both_right += 1
    elif q_ok and not r_ok:
        q_right_r_wrong += 1
        if len([s for s in samples if s[0] == "QR"]) < 6:
            samples.append(("QR", A, B, c, [hex(tq), hex(tr)]))
    elif (not q_ok) and r_ok:
        q_wrong_r_right += 1
    else:
        both_wrong += 1
        if len([s for s in samples if s[0] == "BOTH"]) < 6:
            samples.append(("BOTH", A, B, c, [hex(tq), hex(tr)]))

print(f"GEN={GEN} SEED={SEED} T={len(cases)} both_right={both_right} "
      f"q_right_r_wrong={q_right_r_wrong} q_wrong_r_right={q_wrong_r_right} both_wrong={both_wrong}")
for s in samples:
    print("SAMPLE", s[0])
    print("  A=", s[1][:50], "...")
    print("  B=", s[2][:50], "...")
    print("  got=", s[3])
    print("  true=", s[4])
