# AZZRCN
# https://github.com/AZZRCN
# Q1 Step 0: cachegrind D refs 比对 (确定性, 零抖动, 可外推 Zen3)
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
def cg(b, case, ll, assoc):
    log, out = f"/tmp/cg_{b}_{case}.log", f"/tmp/cg_{b}_{case}.out"
    rc, o, _ = vmctl.run(
        f"cd {H} && rm -f {out} {log} && valgrind --tool=cachegrind --cache-sim=yes "
        f"--I1=32768,8,64 --D1=32768,8,64 --LL={ll},{assoc},64 "
        f"--cachegrind-out-file={out} --log-file={log} build/{b} < data/_big/{case}.in > /dev/null 2>&1; "
        f"grep -E 'refs:|misses:' {log} | sed 's/==[0-9]*== //'")
    d = {}
    for line in (o or "").splitlines():
        m = re.match(r"\s*([A-Za-z0-9 ]+?):\s+([\d,]+)", line)
        if m:
            d[m.group(1).strip()] = int(m.group(2).replace(",", ""))
    return d
rows = []
for case in ["pow2_big", "mix3_big"]:
    for (ll, a, tag) in [(524288, 8, "L2"), (33554432, 18, "L3")]:
        x, y = cg("v13w", case, ll, a), cg("v16", case, ll, a)
        rows.append((case, tag, x, y))
print("\n\n########## cachegrind (确定性) ##########")
keys = ["I refs", "D refs", "D1 misses", "LLd misses", "LL misses"]
for case, tag, x, y in rows:
    print(f"\n### {case} [{tag}]")
    print("   %-14s %16s %16s %9s" % ("metric", "v13w", "v16", "v16/v13w"))
    for k in keys:
        if k in x and k in y:
            print("   %-14s %16d %16d %9.4f" % (k, x[k], y[k], y[k] / x[k] if x[k] else 0))
