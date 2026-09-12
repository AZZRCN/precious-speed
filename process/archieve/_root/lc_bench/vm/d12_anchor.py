#!/usr/bin/env python3
"""锚点测速: div_orig / div_D4 / div_D11 / div_D12 全用例绝对耗时 + 配对比值。

为什么要锚点
------------
D10/D11 从未提交过 LC, 用它们当基准做链式外推等于空中楼阁。
仅有的两个**已确认** LC 锚点:
    真原版 best/div.cpp  -> #389360 = 81ms
    D4     best/div_D4.cpp -> #390120 = 75ms (瓶颈 length_ratio_integer_02)
故预测走: LC(D12) ~ 75ms * [max_case(D12) / max_case(D4)]  (同机同编译, 比值迁移)

纪律: 预热丢弃 + 交替运行 + rep 轮转 (见 divratio.py)。绝对值仅在 VM 空闲时可信。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 7

SRCS = {
    "div_orig": "div.cpp",       # 真原版 = LC 81ms
    "div_D4":   "div_D4.cpp",    # LC 75ms
    "div_D11":  "div_D11.cpp",
    "div_D12":  "div_D12.cpp",
}

for name, fn in SRCS.items():
    p = os.path.join(PS, "best", fn)
    if os.path.exists(p):
        put(p, f"/home/azzr/divbench/src/{name}.cpp")

run("cd ~/divbench/src && for n in " + " ".join(SRCS) + "; do "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/$n $n.cpp || echo BUILDFAIL_$n; done; echo built",
    timeout=1800)

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, time, statistics, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = ["div_orig", "div_D4", "div_D11", "div_D12"]
REPS = %d
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
t = {(c, b): [] for c in cases for b in BINS}

# 预热 (丢弃)
for b in BINS:
    for c in cases[:3]:
        with open(IN + c + '.in', 'rb') as f:
            subprocess.run(['./bin/' + b], stdin=f, stdout=subprocess.DEVNULL)

# 交替运行 + 每 rep 轮转 bin 顺序, 消位置偏置
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
print(f"{'case':<30}" + "".join(f"{b:>11}" for b in BINS))
print('-' * (30 + 11 * len(BINS)))
for c in cases:
    print(f"{c:<30}" + "".join(f"{med[(c,b)]:>11.1f}" for b in BINS))
print('-' * (30 + 11 * len(BINS)))
worst = {b: max(med[(c, b)] for c in cases) for b in BINS}
wcase = {b: max(cases, key=lambda c: med[(c, b)]) for b in BINS}
print(f"{'MAX over cases':<30}" + "".join(f"{worst[b]:>11.1f}" for b in BINS))
print(f"{'  (worst case)':<30}" + "".join(f"{wcase[b][:10]:>11}" for b in BINS))
print()
print('== LC 外推 (锚点: div_orig=81ms #389360, div_D4=75ms #390120) ==')
for b in BINS:
    r_orig = worst[b] / worst['div_orig'] * 81.0
    r_d4 = worst[b] / worst['div_D4'] * 75.0
    print(f'  {b:<10} maxlocal={worst[b]:7.1f}ms   via_orig={r_orig:5.1f}ms   via_D4={r_d4:5.1f}ms')
EOF''' % REPS

run(remote, timeout=7200)
