"""扫描 MUL_FFT_LEAF_LOG 各取值在最重用例上的耗时。

交替轮转执行各二进制以抵消机器状态漂移，报告 min / median。
用法: python sweep_leaf.py [rounds] [case...]
"""
import os
import pathlib
import statistics
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).parent
CASE_DIR = HERE / "cases" / "mul"

ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 9
CASE_NAMES = sys.argv[2:] or ["max_max_00", "fft_killer_00", "large_small_00"]

BINARIES = [("gold", HERE / "exe" / "mul_gold.exe")]
_r4 = HERE / "exe" / "mul_r4.exe"
if _r4.exists():
    BINARIES.append(("r4", _r4))
for log in (19, 16, 14, 13, 12, 11, 10, 9, 8):
    path = HERE / "exe" / f"m6_L{log}.exe"
    if path.exists():
        BINARIES.append((f"L{log}", path))
for log in (13, 11, 10, 9):
    path = HERE / "exe" / f"mul_r4dfs_L{log}.exe"
    if path.exists():
        BINARIES.append((f"r4dfs{log}", path))


def resolve(name):
    for candidate in (CASE_DIR / f"{name}.in", CASE_DIR / f"{name} .in"):
        if candidate.exists():
            return candidate
    raise SystemExit(f"case not found: {name}")


def run_once(exe, case):
    with open(case, "rb") as fh:
        start = time.perf_counter()
        proc = subprocess.run([str(exe)], stdin=fh, stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL, timeout=120)
        elapsed = time.perf_counter() - start
    if proc.returncode != 0:
        raise SystemExit(f"{exe} failed rc={proc.returncode}")
    return elapsed * 1000


for case_name in CASE_NAMES:
    case = resolve(case_name)
    samples = {label: [] for label, _ in BINARIES}
    for _, exe in BINARIES:          # warmup
        run_once(exe, case)
    for _ in range(ROUNDS):
        for label, exe in BINARIES:  # 轮转交替
            samples[label].append(run_once(exe, case))

    base_min = min(samples["gold"])
    print(f"\n=== {case_name}  (rounds={ROUNDS}) ===")
    print(f"{'variant':<8} {'min':>8} {'median':>8} {'vs gold min':>12}")
    print("-" * 40)
    for label, _ in BINARIES:
        vals = samples[label]
        lo, med = min(vals), statistics.median(vals)
        delta = (lo - base_min) / base_min * 100
        mark = "  <<<" if lo < base_min * 0.995 else ""
        print(f"{label:<8} {lo:>8.2f} {med:>8.2f} {delta:>+11.1f}%{mark}")
