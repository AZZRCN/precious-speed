#!/usr/bin/env python3
"""D13 落地验证: nb 代价模型 (无任何调试宏) vs D12 冠军 (LC #390291 = 57ms)。

D13 = D12 + absDivMu 的 nb 代价模型:
    qn_mu > len2 分支, GMP 硬公式 nb=(qn_mu-1)/len2+1 只是可行性下界,
    没有做任何代价比较。模型在 [nb, nb+4] 内按
        cost = A*W(Fi) + W(Fi) + W(Fc) + nblk*(2W(Fi)+2W(Fc)),  W(N)=N*log2 N, A=6.8
    选最小, 门限 in_nat>=16384, 安全边际 0.95。

本脚本:
  1. 干净编译 D12 / D13  (-O2 -std=c++23 -march=x86-64-v3)
  2. 26 例逐字节对拍 (md5)
  3. REPS 轮交替计时 + paired ratio
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 9

SRCS = {"d12": "div_D12.cpp", "d13": "div_D13.cpp"}
for name, fn in SRCS.items():
    put(os.path.join(PS, "best", fn), f"/home/azzr/divbench/src/{name}.cpp")

rc, out, err = run(
    "cd ~/divbench/src && for n in d12 d13; do "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/$n $n.cpp || echo BUILDFAIL_$n; done; echo BUILT",
    timeout=3600)
print(out[-4000:])
print(err[-4000:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, time, statistics, os, hashlib
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = ['d12', 'd13']
REPS = %d
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))

print('== correctness: d13 vs d12 (md5) ==')
bad = []
for c in cases:
    hs = []
    for b in BINS:
        with open(IN + c + '.in', 'rb') as f:
            p = subprocess.run(['./bin/' + b], stdin=f, capture_output=True)
        hs.append(hashlib.md5(p.stdout).hexdigest())
    if hs[0] != hs[1]:
        bad.append(c)
print('  mismatches:', bad if bad else 'NONE', flush=True)

t = {(c, b): [] for c in cases for b in BINS}
for b in BINS:
    for c in cases[:3]:
        with open(IN + c + '.in', 'rb') as f:
            subprocess.run(['./bin/' + b], stdin=f, stdout=subprocess.DEVNULL)

for r in range(REPS):
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
print(f"{'case':<30}{'d12':>9}{'d13':>9}{'ratio':>9}{'paired':>9}")
print('-' * 66)
for c in cases:
    rr = [x / y for x, y in zip(t[(c, 'd13')], t[(c, 'd12')]) if y > 0]
    print(f"{c:<30}{med[(c,'d12')]:>9.1f}{med[(c,'d13')]:>9.1f}"
          f"{med[(c,'d13')]/med[(c,'d12')]:>9.3f}{statistics.median(rr):>9.3f}")
print('-' * 66)
for b in BINS:
    w = max(cases, key=lambda c: med[(c, b)])
    print(f"  {b}: MAX = {med[(w,b)]:.1f} ms  @ {w}")
EOF''' % REPS

rc, out, err = run(remote, timeout=9000)
print(out)
with open(os.path.join(PS, "lc_bench", "vm", "_final_d13.log"), "w", encoding="utf-8") as f:
    f.write(out)
