# AZZRCN
# https://github.com/AZZRCN
# 逐函数 Ir 精确 diff (cachegrind, 确定性零抖动) —— 找 v17nm 比 v13w 多的 ~770K 指令。
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"; B = f"{H}/build"
CASE = "data/mul/max_max_00.in"
res = {}
for b in ["v13w", "v17nm"]:
    out_f = f"/tmp/cgfn_{b}.out"
    rc, o, _ = vmctl.run(
        f"cd {H} && rm -f {out_f} && valgrind --tool=cachegrind --cache-sim=no "
        f"--cachegrind-out-file={out_f} {B}/{b} < {CASE} > /dev/null 2>/dev/null; "
        f"cg_annotate --threshold=99.5 {out_f} 2>/dev/null | sed -n '/Ir *file:function/,/^$/p' | head -40")
    print(f"\n===== {b} =====\n{o}")
    d = {}
    for line in (o or "").splitlines():
        m = re.match(r"\s*([\d,]+)\s*\(\s*[\d.]+%\)\s+(.+)$", line)
        if m:
            d[m.group(2).strip()] = int(m.group(1).replace(",", ""))
    res[b] = d
a, c = res.get("v13w", {}), res.get("v17nm", {})
keys = sorted(set(a) | set(c), key=lambda k: -(abs(c.get(k, 0) - a.get(k, 0))))
print("\n########## Ir delta (v17nm - v13w), 按绝对差排序 ##########")
print("%14s %14s %12s  %s" % ("v13w", "v17nm", "delta", "symbol"))
tot = 0
for k in keys[:20]:
    x, y = a.get(k, 0), c.get(k, 0)
    tot += y - x
    print("%14d %14d %+12d  %s" % (x, y, y - x, k[:90]))
print(f"\nsum(top20 delta) = {tot:+d}")
