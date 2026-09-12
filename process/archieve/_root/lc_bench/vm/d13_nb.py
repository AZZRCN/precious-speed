#!/usr/bin/env python3
"""D13 杠杆验证: DIV_MU_NB_DELTA —— absDivMu 块数 nb 的代价权衡。

背景
----
PROFILE_DIV 剖析显示 length_ratio_integer_02 的
  absInvNewton+prepareDFT (倒数计算) = 21.18ms = 40.9% of blocks loop
是最大单一热点; 而 `in` 被 `if (in > len2) in = len2` 硬顶死, 无法缩小。

GMP mu_div_qr 在 qn_mu > len2 分支用硬公式 nb = (qn_mu-1)/len2 + 1,
没有任何代价比较。本实验强制 nb += DELTA:
  nb 变大 -> in = (qn_mu-1)/nb + 1 变小 -> 倒数(mu)更便宜
  但块数变多 -> 块内 FFT 次数变多
测量该权衡的最优点。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
DELTAS = [0, 1, 2, 3, 4]
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 7

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/div_D13.cpp")

# ---- 编译各 DELTA 档 ----
build = "cd ~/divbench/src && "
for d in DELTAS:
    if d == 0:
        build += ("g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/nb0 div_D13.cpp "
                  "|| echo BUILDFAIL_0; ")
    else:
        build += (f"g++ -O2 -std=c++23 -march=x86-64-v3 -DDIV_MU_NB_DELTA={d} "
                  f"-o ../bin/nb{d} div_D13.cpp || echo BUILDFAIL_{d}; ")
build += "echo BUILT"
rc, out, err = run(build, timeout=3600)
print(out[-3000:])
if "BUILT" not in out:
    sys.exit("build failed")

BINS = ["nb%d" % d for d in DELTAS]

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, time, statistics, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
OUTDIR = '/home/azzr/lcp/big_integer/division_of_big_integers/out/'
BINS = %r
REPS = %d
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))

# ---- 先做正确性快检 (对拍 nb0) ----
print('== correctness vs nb0 ==')
bad = []
for c in cases:
    ref = None
    for b in BINS:
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        h = hashlib.md5(p.stdout).hexdigest()
        if ref is None:
            ref = h
        elif h != ref:
            bad.append((c, b))
print('  mismatches:', bad if bad else 'NONE')

t = {(c, b): [] for c in cases for b in BINS}
for b in BINS:                      # 预热 (丢弃)
    for c in cases[:3]:
        with open(IN + c + '.in', 'rb') as f:
            subprocess.run(['./bin/' + b], stdin=f, stdout=subprocess.DEVNULL)

for r in range(REPS):               # 交替 + rep 轮转消位置偏置
    order = BINS[r %% len(BINS):] + BINS[:r %% len(BINS)]
    for c in cases:
        for b in order:
            with open(IN + c + '.in', 'rb') as f:
                t0 = time.perf_counter()
                subprocess.run(['./bin/' + b], stdin=f, stdout=subprocess.DEVNULL)
                t[(c, b)].append((time.perf_counter() - t0) * 1000)
    print('rep %%d/%%d' %% (r + 1, REPS), flush=True)

med = {k: statistics.median(v) for k, v in t.items()}
print()
print(f"{'case':<30}" + "".join(f"{b:>9}" for b in BINS) + "    best")
print('-' * (30 + 9 * len(BINS) + 9))
for c in cases:
    bb = min(BINS, key=lambda b: med[(c, b)])
    star = '' if bb == 'nb0' else '  *'
    print(f"{c:<30}" + "".join(f"{med[(c,b)]:>9.1f}" for b in BINS) + f"{bb:>8}{star}")
print('-' * (30 + 9 * len(BINS) + 9))
worst = {b: max(med[(c, b)] for c in cases) for b in BINS}
wcase = {b: max(cases, key=lambda c: med[(c, b)]) for b in BINS}
print(f"{'MAX over cases':<30}" + "".join(f"{worst[b]:>9.1f}" for b in BINS))
print(f"{'  (worst case)':<30}" + "".join(f"{wcase[b][:8]:>9}" for b in BINS))

print()
print('== paired ratio vs nb0 (median of per-rep ratios) ==')
print(f"{'case':<30}" + "".join(f"{b:>9}" for b in BINS[1:]))
for c in cases:
    row = ''
    for b in BINS[1:]:
        rr = [x / y for x, y in zip(t[(c, b)], t[(c, 'nb0')]) if y > 0]
        row += f"{statistics.median(rr):>9.3f}"
    print(f"{c:<30}" + row)
EOF''' % (BINS, REPS)

rc, out, err = run(remote, timeout=9000)
print(out)
with open(os.path.join(PS, "lc_bench", "vm", "_nb_d13.log"), "w", encoding="utf-8") as f:
    f.write(out)
