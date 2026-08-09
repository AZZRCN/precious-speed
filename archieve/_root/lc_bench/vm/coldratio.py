"""VM 端冷启动基准（配对比值版）。

设计纪律（用户强制要求）：
  1. 必须预热（page cache + 频率爬坡），预热样本全部丢弃。
  2. 必须交替运行：同一 (rep, case) 内所有候选背靠背执行，且执行顺序按 rep
     轮转，消除"第一个跑的候选"位置偏置。
  3. **禁止基于绝对时间比较**。主指标一律是同一 (rep, case) 元组内与基准候选
     配对得到的比值 t_cand / t_base；先算比值再取中位数（median-of-ratios），
     这样机器的热衰退/后台干扰会在配对中被约掉。
     反例：先对绝对时间取中位数再相除（ratio-of-medians）无法约掉漂移。
  4. 绝对时间只作为"漂移诊断"输出，且必须与相对值同时出现，禁止单独引用。

用法：
    python3 coldratio.py <reps> <baseline_cand> <cand2> [cand3 ...]
基准候选是第一个候选，比值恒为 1.000。
"""
import json
import os
import statistics
import subprocess
import sys
import time

CASES_DIR = "/home/azzr/mulbench/cases"
BIN_DIR = "/home/azzr/mulbench/bin"
CASES = [
    "example_00.in", "small_00.in",
    "medium_00.in", "medium_01.in", "medium_02.in",
    "large_00.in", "large_01.in", "large_02.in",
    "max_max_00.in", "max_max_01.in", "max_max_02.in", "max_max_03.in",
    "max_max_04.in", "max_max_05.in", "max_max_06.in", "max_max_07.in",
    "zero_00.in", "fft_killer_00.in", "fft_killer_01.in", "large_small_00.in",
]

REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 9
CANDS = sys.argv[2:]
if len(CANDS) < 2:
    sys.exit("need >= 2 candidates; first one is the baseline")
BASE = CANDS[0]


def run_once(cand, case):
    path = os.path.join(BIN_DIR, cand)
    with open(os.path.join(CASES_DIR, case), "rb") as fh:
        start = time.perf_counter()
        subprocess.run(["taskset", "-c", "0", path], stdin=fh,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return (time.perf_counter() - start) * 1000.0


# ---- 预热：让 page cache 命中、CPU 频率进入稳态，样本全丢 ----
for _ in range(2):
    for case in CASES:
        for cand in CANDS:
            run_once(cand, case)
print("warmup done", flush=True)

# ratios[cand][case] = [每个 rep 的配对比值]
ratios = {c: {case: [] for case in CASES} for c in CANDS}
# abs_ms 仅用于漂移诊断
abs_ms = {c: {case: [] for case in CASES} for c in CANDS}

for rep in range(REPS):
    for case in CASES:
        # 顺序按 rep 轮转，消除位置偏置
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
print("== 主表：per-case 配对比值中位数（median-of-ratios，baseline=%s=1.000）==" % BASE)
header = f"{'case':<20}" + "".join(c.replace('v3_', '').rjust(10) for c in CANDS)
print(header)
print("-" * len(header))
for case in CASES:
    row = f"{case:<20}"
    for cand in CANDS:
        row += f"{med(ratios[cand][case]):10.3f}"
    print(row)

# LC 的分数由最慢用例决定；先用 baseline 找出瓶颈用例，再看该用例上的比值。
bottleneck = max(CASES, key=lambda c: med(abs_ms[BASE][c]))
print()
print(f"== LC 代理指标：瓶颈用例 = {bottleneck}（由 baseline 的最慢用例确定）==")
print(f"{'cand':<12}{'ratio@bneck':>13}{'gain':>9}{'worstRatio':>12}{'p25':>8}{'p75':>8}")
print("-" * 62)
summary = {}
for cand in CANDS:
    rb = med(ratios[cand][bottleneck])
    # 跨所有用例中最差（最大）的比值，用于确认没有某个用例被拖慢
    wr = max(med(ratios[cand][c]) for c in CASES)
    qs = sorted(ratios[cand][bottleneck])
    p25 = qs[len(qs) // 4]
    p75 = qs[(len(qs) * 3) // 4]
    summary[cand] = {"ratio_bottleneck": rb, "worst_case_ratio": wr,
                     "p25": p25, "p75": p75}
    print(f"{cand:<12}{rb:13.3f}{(rb - 1) * 100:+8.1f}%{wr:12.3f}{p25:8.3f}{p75:8.3f}")

# ---- 漂移诊断：baseline 每个 rep 的绝对时间，归一化到 rep1 ----
print()
print("== 漂移诊断：baseline 在瓶颈用例上的逐 rep 绝对时间（归一化到 rep1）==")
series = abs_ms[BASE][bottleneck]
print("rep      " + "".join(f"{i + 1:>8}" for i in range(len(series))))
print("norm     " + "".join(f"{t / series[0]:8.3f}" for t in series))
drift = (max(series) - min(series)) / min(series) * 100
print(f"漂移幅度 = {drift:.1f}%  （绝对时间仅供诊断，禁止用于候选间比较）")

json.dump({"reps": REPS, "baseline": BASE, "bottleneck": bottleneck,
           "summary": summary,
           "ratios": {c: ratios[c] for c in CANDS},
           "abs_ms_diagnostic_only": {c: abs_ms[c] for c in CANDS}},
          open("/home/azzr/mulbench/coldratio.json", "w"), indent=1)
print("\nwrote coldratio.json")
