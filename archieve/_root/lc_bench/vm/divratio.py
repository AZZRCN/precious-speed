"""VM 端 DIV 冷启动基准（配对比值版）。纪律同 coldratio.py：

  1. 必须预热（page cache + 频率爬坡），预热样本全部丢弃。
  2. 必须交替运行：同一 (rep, case) 内所有候选背靠背执行，顺序按 rep 轮转。
  3. 禁止基于绝对时间比较。主指标 = 同一 (rep, case) 内与基准配对的比值
     t_cand / t_base，先算比值再取中位数（median-of-ratios）。
  4. 绝对时间只作漂移诊断输出。

用法: python3 divratio.py <reps> <baseline_bin> <cand2> [cand3 ...]
"""
import json
import os
import statistics
import subprocess
import sys
import time

CASES_DIR = "/home/azzr/lcp/big_integer/division_of_big_integers/in"
BIN_DIR = "/home/azzr/divbench/bin"
# 只跑对 LC 计分有意义的重用例（LC 分数 = 最慢用例）
CASES = [
    "length_ratio_integer_00.in", "length_ratio_integer_01.in",
    "length_ratio_integer_02.in", "length_ratio_integer_03.in",
    "length_ratio_integer_04.in", "length_ratio_integer_05.in",
    "a_max_b_random_00.in", "a_max_b_random_01.in", "a_max_b_random_02.in",
    "r_nearly_zero_00.in", "r_nearly_zero_01.in", "r_nearly_zero_02.in",
    "burnikel_ziegler_bound_00.in", "burnikel_ziegler_bound_01.in",
    "burnikel_ziegler_bound_02.in", "burnikel_ziegler_bound_03.in",
    "large_00.in", "large_01.in",
    "medium_01.in", "medium_02.in",
    "max_00.in", "max_01.in", "max_02.in",
]

REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 7
CANDS = sys.argv[2:]
if len(CANDS) < 2:
    sys.exit("need >= 2 candidates; first one is the baseline")
BASE = CANDS[0]

CASES = [c for c in CASES if os.path.exists(os.path.join(CASES_DIR, c))]


def run_once(cand, case):
    path = os.path.join(BIN_DIR, cand)
    with open(os.path.join(CASES_DIR, case), "rb") as fh:
        start = time.perf_counter()
        subprocess.run(["taskset", "-c", "0", path], stdin=fh,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return (time.perf_counter() - start) * 1000.0


for _ in range(2):
    for case in CASES:
        for cand in CANDS:
            run_once(cand, case)
print("warmup done", flush=True)

ratios = {c: {case: [] for case in CASES} for c in CANDS}
abs_ms = {c: {case: [] for case in CASES} for c in CANDS}

for rep in range(REPS):
    for case in CASES:
        order = CANDS[rep % len(CANDS):] + CANDS[:rep % len(CANDS)]
        tup = {}
        for cand in order:
            tup[cand] = run_once(cand, case)
        base_t = tup[BASE]
        for cand in CANDS:
            abs_ms[cand][case].append(tup[cand])
            ratios[cand][case].append(tup[cand] / base_t)
    print(f"rep {rep + 1}/{REPS} done", flush=True)


def med(xs):
    return statistics.median(xs)


print()
print("== 主表：per-case 配对比值中位数（baseline=%s=1.000）==" % BASE)
header = f"{'case':<30}" + "".join(c.replace('div_', '').rjust(11) for c in CANDS)
print(header)
print("-" * len(header))
for case in CASES:
    row = f"{case[:-3]:<30}"
    for cand in CANDS:
        row += f"{med(ratios[cand][case]):11.3f}"
    print(row)

bottleneck = max(CASES, key=lambda c: med(abs_ms[BASE][c]))
print()
print(f"== LC 代理指标：瓶颈用例 = {bottleneck}（由 baseline 最慢用例确定）==")
print(f"{'cand':<14}{'ratio@bneck':>13}{'gain':>9}{'worstRatio':>12}{'worstCase':>28}")
print("-" * 78)
summary = {}
for cand in CANDS:
    rb = med(ratios[cand][bottleneck])
    wcase = max(CASES, key=lambda c: med(ratios[cand][c]))
    wr = med(ratios[cand][wcase])
    summary[cand] = {"ratio_bottleneck": rb, "worst_case_ratio": wr,
                     "worst_case": wcase}
    print(f"{cand:<14}{rb:13.3f}{(rb - 1) * 100:+8.1f}%{wr:12.3f}{wcase[:-3]:>28}")

print()
print("== 漂移诊断：baseline 在瓶颈用例上的逐 rep 绝对时间（归一化到 rep1）==")
series = abs_ms[BASE][bottleneck]
print("rep      " + "".join(f"{i + 1:>8}" for i in range(len(series))))
print("norm     " + "".join(f"{t / series[0]:8.3f}" for t in series))
drift = (max(series) - min(series)) / min(series) * 100
print(f"漂移幅度 = {drift:.1f}%  （绝对时间仅供诊断，禁止用于候选间比较）")

json.dump({"reps": REPS, "baseline": BASE, "bottleneck": bottleneck,
           "summary": summary, "ratios": ratios},
          open("/home/azzr/divbench/divratio.json", "w"), indent=1)
print("\nwrote divratio.json")
