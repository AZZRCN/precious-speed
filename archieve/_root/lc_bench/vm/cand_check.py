#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通用候选检查器 —— 取代一次性的 dNN_check.py。

  cand_check.py "D20b,D20c" [cases] [baselines] [tag]

  1. 上传 best/div_<C>.cpp -> VM, 编译成 bin/<c>  (c = C.lower())
  2. 26 例官方输入逐字节对拍 (vs d13, 已知正确基线)
  3. callgrind Zen3 est 对比 baselines + 候选

默认 cases  = length_ratio_integer_03,r_nearly_zero_01   (D19 后的两大瓶颈)
默认 basel. = d19
全程零 wall-clock —— callgrind 是确定性的, 单轮即终值。
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
FLAGS = "-O2 -std=c++23 -march=x86-64-v3"

CANDS = [c.strip() for c in (sys.argv[1] if len(sys.argv) > 1 else "D20b").split(",")]
CASES = sys.argv[2] if len(sys.argv) > 2 else \
    "length_ratio_integer_03,r_nearly_zero_01"
BASES = [b.strip() for b in (sys.argv[3] if len(sys.argv) > 3 else "d19").split(",")]
TAG = sys.argv[4] if len(sys.argv) > 4 else "q"

bins = [c.lower() for c in CANDS]

# ---- 1. upload + build ------------------------------------------------
for C, b in zip(CANDS, bins):
    src = os.path.join(PS, "best", "div_%s.cpp" % C)
    if not os.path.exists(src):
        sys.exit("missing %s" % src)
    put(src, "/home/azzr/divbench/src/%s.cpp" % b)
    print("[up] %s -> %s.cpp" % (os.path.basename(src), b))

build = " && ".join("g++ %s -o ../bin/%s %s.cpp" % (FLAGS, b, b) for b in bins)
rc, out, err = run("cd ~/divbench/src && %s && echo BUILT" % build, timeout=7200)
print(out[-1500:])
if "BUILT" not in out:
    print(err[-6000:])
    sys.exit("build failed")
print("[ok] built: %s" % ", ".join(bins))

# ---- 2. 26-case byte-exact diff vs d13 --------------------------------
md5 = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
TARGETS = %r
ref = {}
for c in cases:
    with open(IN + c + '.in', 'rb') as f:
        p = subprocess.run(['./bin/d13'], stdin=f, capture_output=True)
    ref[c] = hashlib.md5(p.stdout).hexdigest()
for b in TARGETS:
    bad = []
    for c in cases:
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        if hashlib.md5(p.stdout).hexdigest() != ref[c]:
            bad.append(c)
    print('== %%s vs d13 (26 cases): %%s' %% (b, bad if bad else 'ALL MATCH'))
EOF''' % (bins,)
rc, o0, err = run(md5, timeout=7200)
print(o0)
if err.strip():
    print(err[-1500:])

# ---- 3. callgrind Zen3 est --------------------------------------------
put(os.path.join(HERE, "_d17_cg_remote.py"),
    "/home/azzr/divbench/_d17_cg_remote.py")
allbins = ",".join(BASES + bins)
rc, o1, err = run("cd ~/divbench && python3 _d17_cg_remote.py '%s' '%s'"
                  % (allbins, CASES), timeout=21600)
print(o1)

log = os.path.join(HERE, "_cand_%s.log" % TAG)
with open(log, "w", encoding="utf-8") as f:
    f.write(o0 + "\n" + o1)
print("[log] %s" % log)
