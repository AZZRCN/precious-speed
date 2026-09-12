#!/usr/bin/env python3
"""锚点测速 v2: 加入 div_D10 (LC #390217 = 69ms) 作最近同族锚点 + 逐点比值外推。

为什么要 v2
-----------
v1 只用 div_orig(81ms)/div_D4(75ms) 两锚点, 但二者本地 maxlocal 几乎相同
(64.9 vs 64.7) 而 LC 相差 81 vs 75 -> 单一 max-case 外推存在系统误差。
div_D10 是**最近一次已确认**的 LC 提交 (#390217 = 69ms AC), 且与 D11/D12
同代码族 (同 Newton cyclic 阶梯), 迁移率最接近, 是最可信的外推底座。

逐点外推:  LC(D12, case) ~ LC(D10, case) * local(D12,case) / local(D10,case)
已知 D10 的 LC 逐点耗时 (回执): length_ratio_integer_02=69, r_nearly_zero_01=69,
a_max_b_random_02=68。其余点用全局 69 * local 比例兜底。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 9

SRCS = {
    "div_orig": "div.cpp",       # 真原版 = LC #389360 81ms
    "div_D4":   "div_D4.cpp",    # LC #390120 75ms
    "div_D10":  "div_D10.cpp",   # LC #390217 69ms  <- 最近同族锚点
    "div_D11":  "div_D11.cpp",   # 未提交
    "div_D12":  "div_D12.cpp",   # 未提交 (候选)
}

for name, fn in SRCS.items():
    p = os.path.join(PS, "best", fn)
    if os.path.exists(p):
        put(p, f"/home/azzr/divbench/src/{name}.cpp")
    else:
        print("MISSING", p)

run("cd ~/divbench/src && for n in " + " ".join(SRCS) + "; do "
    "g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/$n $n.cpp || echo BUILDFAIL_$n; done; echo built",
    timeout=2400)

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, time, statistics, os
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
BINS = ["div_orig", "div_D4", "div_D10", "div_D11", "div_D12"]
REPS = %d
# D10 的 LC 回执逐点耗时 (#390217, 总 69ms)
LC_D10 = {"length_ratio_integer_02": 69.0, "r_nearly_zero_01": 69.0, "a_max_b_random_02": 68.0}
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
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
W = 30 + 10 * len(BINS)
print()
print(f"{'case':<30}" + "".join(f"{b.replace('div_',''):>10}" for b in BINS) + "   D12/D10")
print('-' * (W + 10))
for c in cases:
    ratio = med[(c, 'div_D12')] / med[(c, 'div_D10')]
    print(f"{c:<30}" + "".join(f"{med[(c,b)]:>10.1f}" for b in BINS) + f"{ratio:>10.3f}")
print('-' * (W + 10))
worst = {b: max(med[(c, b)] for c in cases) for b in BINS}
wcase = {b: max(cases, key=lambda c: med[(c, b)]) for b in BINS}
print(f"{'MAX over cases':<30}" + "".join(f"{worst[b]:>10.1f}" for b in BINS))
print(f"{'  (worst case)':<30}" + "".join(f"{wcase[b][:9]:>10}" for b in BINS))

print()
print('== 全局 max-case 外推 (3 锚点) ==')
for b in BINS:
    print(f'  {b:<9} maxlocal={worst[b]:6.1f}  via_orig(81)={worst[b]/worst["div_orig"]*81:5.1f}'
          f'  via_D4(75)={worst[b]/worst["div_D4"]*75:5.1f}'
          f'  via_D10(69)={worst[b]/worst["div_D10"]*69:5.1f}')

print()
print('== 逐点外推 (以 D10 LC 回执为底, 仅已知点) ==')
best = 0.0
for c, lc10 in sorted(LC_D10.items()):
    if c not in cases:
        print(f'  {c:<28} MISSING locally'); continue
    for b in ("div_D11", "div_D12"):
        p = lc10 * med[(c, b)] / med[(c, 'div_D10')]
        if b == "div_D12":
            best = max(best, p)
        print(f'  {c:<28} {b:<9} LC(D10)={lc10:4.0f} -> pred {p:5.1f}ms'
              f'   (local {med[(c,b)]:.1f} / {med[(c,"div_D10")]:.1f})')
print(f'\n  => D12 逐点外推上界 (已知瓶颈点): {best:.1f}ms   [D10 实测 69ms]')
EOF''' % REPS

run(remote, timeout=7200)
