#!/usr/bin/env python3
"""D13: 用 INVPROF + PROFILE_DIV 标定「倒数成本 / 块成本」的代价模型常数 A。

模型:  cost(nblk) = A*W(Fi) + nblk*(2W(Fl)+2W(Fc)) + W(Fi) + W(Fc|Fl)
       W(N) = N*log2(N),  in = ceil(qn/nblk)
标定用例: a_max_b_random_02 (delta 0/1 时 Fi 393216->196608, 实测 -21.6%)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
CASES = sys.argv[1:] or ["a_max_b_random_02", "length_ratio_integer_02",
                         "length_ratio_integer_01"]
DELTAS = [0, 1]

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")

build = "cd ~/divbench/src && "
for d in DELTAS:
    flag = "" if d == 0 else f"-DDIV_MU_NB_DELTA={d} "
    build += (f"g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV -DINVPROF {flag}"
              f"-o ../bin/ip{d} div_D13.cpp || echo BUILDFAIL_{d}; ")
build += "echo BUILT"
rc, out, err = run(build, timeout=3600)
print(out[-2500:])
if "BUILT" not in out:
    sys.exit("build failed")

REMOTE = r'''
import re, subprocess, sys
from collections import defaultdict
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
for CASE in sys.argv[1:]:
    print('\n############### %s ###############' % CASE)
    for d in [0, 1]:
        pf = Path('/home/azzr/divbench/prof_detail.log')
        if pf.exists():
            pf.unlink()
        with open(IN / (CASE + '.in'), 'rb') as f:
            p = subprocess.run(['/home/azzr/divbench/bin/ip%d' % d], stdin=f,
                               capture_output=True, timeout=900,
                               cwd='/home/azzr/divbench')
        err = p.stderr.decode('utf-8', 'replace')
        prof = pf.read_text(errors='replace') if pf.exists() else ''
        agg = defaultdict(lambda: [0.0, 0])
        for l in prof.splitlines():
            m = re.search(r'\[prof\]\s+(.*?):\s+([\d.]+)\s*ms', l)
            if not m:
                continue
            lab = re.sub(r'block \d+', 'block N', m.group(1))
            lab = re.sub(r'\(.*?\)', '', lab).strip()
            agg[lab][0] += float(m.group(2))
            agg[lab][1] += 1
        print('  ===== delta=%d =====' % d)
        for k in ('absDivMu: total', 'absDivMu: blocks loop',
                  'absDivMu: absInvNewton+prepareDFT',
                  'block N: fftMulPre #1', 'block N: fftMulModBm1Pre #2'):
            if k in agg:
                print('    %-40s %9.3f ms  n=%d' % (k, agg[k][0], agg[k][1]))
        for l in err.splitlines():
            if '[invprof]' in l or '[invph]' in l:
                print('    ' + l.replace('[invprof] ', 'INV ').replace('[invph] ', 'PH  '))
'''
tmp = os.path.join(PS, "lc_bench", "vm", "_invprof_remote.py")
with open(tmp, "w", encoding="utf-8") as f:
    f.write(REMOTE)
put(tmp, "/tmp/invprof_remote.py")
rc, out, err = run("python3 /tmp/invprof_remote.py " + " ".join(CASES), timeout=2400)
print(out)
with open(os.path.join(PS, "lc_bench", "vm", "_invprof_d13.log"), "w", encoding="utf-8") as f:
    f.write(out)
