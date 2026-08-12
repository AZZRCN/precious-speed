# AZZRCN
# https://github.com/AZZRCN
# 确认修复: 混合档 (mix3/mix5) 的 I refs / D refs 是否从 v16 的 1.27 / 1.19 回落到 ~1.0。
# cachegrind 是确定性的, 架构量可外推 Zen3。
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"

def cg(b, c):
    log = f"/tmp/c_{b}_{c}.log"
    rc, o, _ = vmctl.run(
        f"cd {H} && rm -f {log} && valgrind --tool=cachegrind --cache-sim=yes "
        f"--I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 --log-file={log} "
        f"build/{b} < data/_big/{c}.in > /dev/null 2>&1; "
        f"grep -E 'refs:|misses:' {log} | sed 's/==[0-9]*== //'")
    d = {}
    for ln in (o or "").splitlines():
        m = re.match(r"\s*([A-Za-z0-9 ]+?):\s+([\d,]+)", ln)
        if m:
            d[m.group(1).strip()] = int(m.group(2).replace(",", ""))
    return d

print("\n########## cachegrind 混合档修复核验 (L2 视角) ##########")
print("%-10s %-8s %14s %14s %14s %9s %9s" % ("case", "metric", "v13w", "v16", "v18", "16/13w", "18/13w"))
for c in ["pow2_big", "mix3_big", "mix5_big"]:
    x, y, z = cg("v13w", c), cg("v16", c), cg("v18", c)
    for k in ["I refs", "D refs", "D1  misses", "LLd misses"]:
        if k in x and k in y and k in z:
            print("%-10s %-8s %14d %14d %14d %9.4f %9.4f" %
                  (c, k, x[k], y[k], z[k], y[k] / x[k], z[k] / x[k]))
