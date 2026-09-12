import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = "/home/azzr/hexbench"; B = f"{H}/build"; S = f"{H}/scratch"
CXX = "g++ -O2 -march=x86-64-v3 -std=c++23"
vmctl.put(os.path.join(R, "work", "mul", "v19.cpp"), f"{S}/v19.cpp")
rc, out, _ = vmctl.run(f"cd {S} && {CXX} v19.cpp -o {B}/v19 2>&1 | grep -v Wignored | grep -v '^ ' | grep -v '^\s*|' | head -10; echo RC=${{PIPESTATUS[0]}}")
print(out)
vmctl.run(f"cd {H} && bad=0; for f in data/mul/*.in; do e=${{f%.in}}.exp; [ -f \"$e\" ] || continue; "
          f"if ! build/v19 < $f | cmp -s - $e; then echo \"BAD $(basename $f)\"; bad=$((bad+1)); fi; done; echo \"v19 gate bad=$bad\"")
def cg(b, c):
    log = f"/tmp/n_{b}_{c}.log"
    rc, o, _ = vmctl.run(f"cd {H} && rm -f {log} && valgrind --tool=cachegrind --cache-sim=yes "
        f"--I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 --log-file={log} "
        f"build/{b} < data/_big/{c}.in > /dev/null 2>&1; grep -E 'refs:|misses:' {log} | sed 's/==[0-9]*== //'")
    d = {}
    for ln in (o or "").splitlines():
        m = re.match(r"\s*([A-Za-z0-9 ]+?):\s+([\d,]+)", ln)
        if m: d[m.group(1).strip()] = int(m.group(2).replace(",", ""))
    return d
BS = ["v16", "v18", "v19"]
for c in ["pow2_big", "mix3_big", "mix5_big"]:
    res = {b: cg(b, c) for b in BS}
    print(f"\n--- {c} ---")
    print("%-12s %14s %14s %14s   %8s" % ("metric", *BS, "19/16"))
    for k in ["I refs", "D refs", "D1  misses", "LLd misses"]:
        if all(k in res[b] for b in BS):
            print("%-12s %14d %14d %14d   %8.4f" % (k, *[res[b][k] for b in BS], res["v19"][k]/res["v16"][k]))
