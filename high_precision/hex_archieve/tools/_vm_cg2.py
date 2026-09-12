# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
def cg(binname, case, ll, assoc):
    log = f"/tmp/cg_{binname}_{case}.log"
    out = f"/tmp/cg_{binname}_{case}.out"
    vmctl.run(f"cd {H} && rm -f {out} {log} && valgrind --tool=cachegrind --cache-sim=yes "
              f"--I1=32768,8,64 --D1=32768,8,64 --LL={ll},{assoc},64 "
              f"--cachegrind-out-file={out} --log-file={log} build/{binname} < data/_big/{case}.in > /dev/null 2>&1; "
              f"grep -E 'D   refs|D1  misses|LLd misses|I   refs' {log}")
for case in ["pow2_big", "mix3_big"]:
    for (ll, a) in [(524288, 8), (33554432, 18)]:
        tag = "L2" if ll == 524288 else "L3"
        print(f"===== {case} [{tag}] =====")
        print("  v13w:", cg("v13w", case, ll, a).replace("\n", "  |  "))
        print("  v16 :", cg("v16", case, ll, a).replace("\n", "  |  "))
