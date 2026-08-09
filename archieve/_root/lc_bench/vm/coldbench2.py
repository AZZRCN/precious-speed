"""VM 端冷启动基准：每次测量一个全新进程，模拟 LC 的单进程判题。

与旧版 vm_coldbench.py 的区别：rep 放在最外层、内层轮转候选，
使各候选处于相近的机器状态，避免时间漂移被误读成性能差异。
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

REPS = int(sys.argv[1]) if len(sys.argv) > 1 else 7
CANDS = sys.argv[2:] or ["v3_gold", "v3_r4", "v3_L19", "v3_L14",
                         "v3_L12", "v3_L11", "v3_L10", "v3_L9"]


def run_once(cand, case):
    path = os.path.join(BIN_DIR, cand)
    with open(os.path.join(CASES_DIR, case), "rb") as fh:
        start = time.perf_counter()
        subprocess.run(["taskset", "-c", "0", path], stdin=fh,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return (time.perf_counter() - start) * 1000.0


samples = {c: {case: [] for case in CASES} for c in CANDS}

for case in CASES:                     # warmup: page cache 预热
    for cand in CANDS:
        run_once(cand, case)

for rep in range(REPS):
    for case in CASES:
        for cand in CANDS:
            samples[cand][case].append(run_once(cand, case))
    print(f"rep {rep + 1}/{REPS} done", flush=True)

print()
header = f"{'case':<20}" + "".join(c.replace('v3_', '').rjust(9) for c in CANDS)
print(header)
print("-" * len(header))
for case in CASES:
    row = f"{case:<20}"
    for cand in CANDS:
        row += f"{statistics.median(samples[cand][case]):9.2f}"
    print(row)

print()
print("LC-worst = max over cases (per-case median / per-case min), ms")
print(f"{'cand':<10}{'worst_med':>11}{'worst_min':>11}{'vs gold':>10}")
print("-" * 42)
summary = {}
base = None
for cand in CANDS:
    worst_med = max(statistics.median(samples[cand][c]) for c in CASES)
    worst_min = max(min(samples[cand][c]) for c in CASES)
    summary[cand] = {"worst_med": worst_med, "worst_min": worst_min}
    if base is None:
        base = worst_med
    print(f"{cand:<10}{worst_med:11.2f}{worst_min:11.2f}"
          f"{(worst_med - base) / base * 100:+9.1f}%")

json.dump({"reps": REPS, "summary": summary,
           "per_case": {c: {k: samples[c][k] for k in CASES} for c in CANDS}},
          open("/home/azzr/mulbench/cold6step.json", "w"), indent=1)
print("\nwrote cold6step.json")
