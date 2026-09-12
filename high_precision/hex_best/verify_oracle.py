#!/usr/bin/env python3
# Oracle 比对: 用 Python 大整数当真值 (截断语义, 与 DEC int64 快速路 C `/%` / LC 约定一致).
#   q = trunc(A/B), r = A - q*B  (0 <= |r| < |B|, r 符号=被除数 A).
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

def run(A, B, nocyclic):
    env = dict(os.environ)
    if nocyclic:
        env["NOCYCLIC"] = "1"
    else:
        env.pop("NOCYCLIC", None)
    r = subprocess.run([divbin], input="1\n" + A + " " + B + "\n",
                       capture_output=True, text=True, timeout=120, env=env)
    return r.stdout.split()

cyc_bad = 0
lin_bad = 0
samples = []
for (A, B) in cases:
    a = int(A, 16); b = int(B, 16)
    # 截断语义 (C/GMP-tdiv): q 符号 = sign(A)^sign(B), r 符号 = sign(A)
    tq = abs(a) // abs(b)
    if (a < 0) != (b < 0):
        tq = -tq
    tr = a - tq * b
    c = run(A, B, False)
    l = run(A, B, True)
    c_ok = len(c) >= 2 and int(c[0], 16) == tq and int(c[1], 16) == tr
    l_ok = len(l) >= 2 and int(l[0], 16) == tq and int(l[1], 16) == tr
    if not c_ok:
        cyc_bad += 1
        if len(samples) < 4:
            samples.append(("CYCLIC", A, B, c, [hex(tq), hex(tr)]))
    if not l_ok:
        lin_bad += 1
        if len(samples) < 4 and not any(s[0] == "LIN" for s in samples):
            samples.append(("LIN", A, B, l, [hex(tq), hex(tr)]))
    if cyc_bad + lin_bad > 5000:
        break

print(f"GEN={GEN} SEED={SEED} T={len(cases)} cyc_bad={cyc_bad} lin_bad={lin_bad}")
for s in samples:
    print("SAMPLE", s[0])
    print("  A=", s[1][:60], "...")
    print("  B=", s[2][:60], "...")
    print("  got=", s[3])
    print("  true=", s[4])
