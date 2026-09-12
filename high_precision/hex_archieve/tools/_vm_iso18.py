# AZZRCN
# https://github.com/AZZRCN
# 隔离: v18 在纯 2 幂路径上比 v16 多 772K I refs / 910K D refs (+0.56% / +2.0%).
# 归因到底是 MR_Tw 两级表 (v18t) 还是 MR_Q O(1) 递推 (v18q)?
# cachegrind 确定性, 只看 pow2_big (headline 代理).
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = "/home/azzr/hexbench"; B = f"{H}/build"; S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
vmctl.run(f"mkdir -p {S} {B}")

for n in ("v18q", "v18t"):
    vmctl.put(os.path.join(R, "work", "mul", f"{n}.cpp"), f"{S}/{n}.cpp")
    rc, out, _ = vmctl.run(f"cd {S} && {CXX} {n}.cpp -o {B}/{n} 2>&1 | head -20; echo RC=${{PIPESTATUS[0]}}")
    print(out)
    if "RC=0" not in (out or ""):
        sys.exit(1)

# 正确性闸门
for n in ("v18q", "v18t"):
    vmctl.run(f"cd {H} && bad=0; for f in data/mul/*.in; do e=${{f%.in}}.exp; [ -f \"$e\" ] || continue; "
              f"if ! build/{n} < $f | cmp -s - $e; then echo \"BAD $(basename $f)\"; bad=$((bad+1)); fi; done; "
              f"echo \"{n} gate bad=$bad\"")

def cg(b, c):
    log = f"/tmp/i_{b}_{c}.log"
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

BS = ["v16", "v18q", "v18t", "v18"]
print("\n########## pow2_big 归因 (cachegrind, 确定性) ##########")
res = {b: cg(b, "pow2_big") for b in BS}
print("%-12s %14s %14s %14s %14s" % ("metric", *BS))
for k in ["I refs", "D refs", "D1  misses", "LLd misses"]:
    if all(k in res[b] for b in BS):
        print("%-12s %14d %14d %14d %14d" % (k, *[res[b][k] for b in BS]))
print("\n--- delta vs v16 ---")
for k in ["I refs", "D refs"]:
    base = res["v16"][k]
    for b in BS[1:]:
        print("%-12s %-6s %+12d  (%+.3f%%)" % (k, b, res[b][k] - base, 100.0 * (res[b][k] - base) / base))
