"""对 26 例官方 case 逐字节比对两个 DIV 可执行文件的输出。
用法: python cmp26.py <ref.exe> <cand.exe> [case_dir]
"""
import hashlib
import os
import subprocess
import sys
import time

ref = os.path.abspath(sys.argv[1])
cand = os.path.abspath(sys.argv[2])
CASE = os.path.abspath(sys.argv[3]) if len(sys.argv) > 3 else \
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "cases_lf", "div")

cases = sorted(f for f in os.listdir(CASE) if f.endswith(".in"))
bad = []
print(f"ref ={ref}\ncand={cand}\ncases={len(cases)} from {CASE}\n")
for c in cases:
    p = os.path.join(CASE, c)
    hs = []
    ts = []
    for exe in (ref, cand):
        t0 = time.perf_counter()
        with open(p, "rb") as fin:
            r = subprocess.run([exe], stdin=fin, stdout=subprocess.PIPE)
        ts.append(time.perf_counter() - t0)
        hs.append(hashlib.md5(r.stdout.replace(b"\r\n", b"\n")).hexdigest())
    ok = hs[0] == hs[1]
    if not ok:
        bad.append(c)
    print(f"{'OK ' if ok else 'FAIL'} {c:34s} ref={ts[0]*1000:8.1f}ms cand={ts[1]*1000:8.1f}ms "
          f"ratio={ts[1]/max(ts[0],1e-9):.3f}")
print()
if bad:
    print(f"*** MISMATCH on {len(bad)} case(s): {bad}")
    sys.exit(1)
print(f"*** ALL {len(cases)} CASES BYTE-IDENTICAL ***")
