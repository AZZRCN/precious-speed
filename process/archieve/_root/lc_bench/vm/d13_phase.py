#!/usr/bin/env python3
"""D13: 用 PROFILE_DIV 剖析 length_ratio_integer_03 / 02 / r_nearly_zero_01 的内部结构。

目的: lri_03 的 FFT 工作量只有 lri_02 的一半, LC 却同为 57ms ->
      非 FFT 部分才是真瓶颈, 找出它。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
CASES = sys.argv[1:] or ["length_ratio_integer_03", "length_ratio_integer_02",
                         "r_nearly_zero_01"]

REMOTE = r'''
import re, subprocess, sys
from collections import defaultdict
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
BIN = Path('/home/azzr/divbench/bin/p_d13')
for CASE in sys.argv[1:]:
    pf = Path('/home/azzr/divbench/prof_detail.log')
    if pf.exists():
        pf.unlink()
    with open(IN / f'{CASE}.in', 'rb') as f:
        p = subprocess.run([str(BIN)], stdin=f, capture_output=True, timeout=900,
                           cwd='/home/azzr/divbench')
    err = p.stderr.decode('utf-8', 'replace')
    if pf.exists():
        err += pf.read_text(errors='replace')
    lines = [l for l in err.splitlines() if '[prof]' in l]
    agg = defaultdict(lambda: [0.0, 0])
    for l in lines:
        m = re.search(r'\[prof\]\s+(.*?):\s+([\d.]+)\s*ms', l)
        if not m:
            continue
        label = re.sub(r'block \d+', 'block N', m.group(1))
        label = re.sub(r'\(.*?\)', '', label).strip()
        agg[label][0] += float(m.group(2))
        agg[label][1] += 1
    tot = sum(v[0] for v in agg.values())
    print(f'\n########## {CASE}   prof lines={len(lines)}  sum={tot:.2f} ms ##########')
    print(f'{"label":<52}{"ms":>10}{"n":>8}{"pct":>8}')
    for k, (ms, n) in sorted(agg.items(), key=lambda kv: -kv[1][0])[:18]:
        print(f'{k[:52]:<52}{ms:10.2f}{n:8d}{(ms/tot*100 if tot else 0):7.1f}%')
    print('--- raw samples ---')
    for l in lines[:12]:
        print('  ' + l.strip()[:145])
'''

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")
rc, out, err = run(
    "cd ~/divbench/src && "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV -o ../bin/p_d13 div_D13.cpp "
    "2>&1 | tail -20 && echo BUILD_DONE", timeout=1200)
print(out)
if "BUILD_DONE" not in out:
    sys.exit("build failed")

tmp = os.path.join(PS, "lc_bench", "vm", "_phase_remote.py")
with open(tmp, "w", encoding="utf-8") as f:
    f.write(REMOTE)
put(tmp, "/tmp/phase_remote.py")
rc, out, err = run("python3 /tmp/phase_remote.py " + " ".join(CASES), timeout=1800)
print(out)
