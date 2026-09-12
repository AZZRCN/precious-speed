#!/usr/bin/env python3
# 逐进程对拍：每个 case 单独起一个 div 二进制进程，隔离 thread_local 跨调用污染。
# 用法: python3 verify_percase.py <genbin> <seed> <divbin> [maxcases]
import subprocess, sys, time

gen = sys.argv[1]
seed = sys.argv[2]
divbin = sys.argv[3]
maxcases = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9

t0 = time.time()
out = subprocess.run([gen, seed], capture_output=True, text=True).stdout
lines = out.split('\n')
T = int(lines[0])
cases = []
i = 1
for _ in range(T):
    if i >= len(lines):
        break
    parts = lines[i].split()
    if len(parts) < 2:
        i += 1
        continue
    cases.append((parts[0], parts[1]))
    i += 1

fails = []
n = min(T, maxcases)
for idx in range(n):
    A, B = cases[idx]
    inp = f"1\n{A} {B}\n"
    r = subprocess.run([divbin], input=inp, capture_output=True, text=True, timeout=30)
    sp = r.stdout.split()
    if len(sp) < 2:
        fails.append((idx, "BADOUT", repr(r.stdout)[:80]))
        continue
    q, rr = sp[0], sp[1]
    a = int(A, 16); b = int(B, 16)
    eq = int(q, 16); er = int(rr, 16)
    if eq != a // b or er != a % b:
        fails.append((idx, A, B, q, rr))

dt = time.time() - t0
print(f"gen={gen} seed={seed} T={T} checked={n} fails={len(fails)} time={dt:.1f}s")
if fails:
    for f in fails[:30]:
        print("FAIL", f)
