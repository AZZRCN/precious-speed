#!/usr/bin/env python3
"""ab_min_bench.py - Interleaved A/B benchmark using MIN-of-N.

Why this exists: daytime_bench.py runs all CUR rounds first, then all BEST
rounds. Any thermal/background drift between the two halves shows up as a
fake per-case delta (observed: two identically-sized max_max cases differing
by 17 ms). LC scores the SLOWEST of 20 cases, so per-case noise matters.

This script:
  * interleaves CUR/BEST within every round (drift cancels),
  * reports min-of-N (robust to OS scheduling spikes) alongside median,
  * subtracts a measured process-spawn floor so the pure compute delta is
    visible (local wall clock carries ~60 ms of spawn+IO that LC does not).

Usage: python ab_min_bench.py [rounds] [case_filter_substring]
Env:   BENCH_CUR / BENCH_BEST override the two executables.
"""
import subprocess, os, time, sys, statistics

CUR = os.environ.get("BENCH_CUR", r"d:\precious_speed\cur_mul.exe")
BEST = os.environ.get("BENCH_BEST", r"d:\precious_speed\best_mul.exe")
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
ROUNDS = int(sys.argv[1]) if len(sys.argv) > 1 else 15
FILTER = sys.argv[2] if len(sys.argv) > 2 else ""

ALL_CASES = [
    "example_00", "small_00", "medium_00", "medium_01", "medium_02",
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "max_max_02", "max_max_03",
    "max_max_04", "max_max_05", "max_max_06", "max_max_07",
    "fft_killer_00", "fft_killer_01", "zero_00",
]
cases = [c for c in ALL_CASES if FILTER in c]


def resolve(c):
    p = os.path.join(CASES_DIR, c + ".in")
    if os.path.exists(p):
        return p
    p2 = os.path.join(CASES_DIR, c + " .in")
    return p2 if os.path.exists(p2) else None


def run_once(exe, inp_path):
    with open(inp_path, "rb") as f:
        t0 = time.perf_counter()
        r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=60)
        t1 = time.perf_counter()
    return (t1 - t0) * 1000 if r.returncode == 0 else float("nan")


resolved = [(c, resolve(c)) for c in cases]
resolved = [(c, p) for c, p in resolved if p]

print(f"CUR  = {CUR}")
print(f"BEST = {BEST}")
print(f"rounds={ROUNDS}, cases={len(resolved)}  (interleaved, min-of-N)\n")

print("Warmup...", flush=True)
for c, p in resolved:
    run_once(CUR, p); run_once(BEST, p)

samples = {c: ([], []) for c, _ in resolved}
for r in range(ROUNDS):
    for c, p in resolved:
        # interleave order flips each round to cancel any ordering bias
        if r & 1:
            b = run_once(BEST, p); a = run_once(CUR, p)
        else:
            a = run_once(CUR, p); b = run_once(BEST, p)
        samples[c][0].append(a); samples[c][1].append(b)
    print(f"  round {r+1}/{ROUNDS}", end="\r", flush=True)
print(" " * 30, end="\r")

# spawn floor: the cheapest case is essentially pure process overhead
floor = min(min(samples[c][0] + samples[c][1]) for c, _ in resolved)

hdr = f"{'case':<18}{'CURmin':>8}{'BSTmin':>8}{'dmin':>7}{'%cmp':>7}   {'CURmed':>8}{'BSTmed':>8}{'wins':>7}"
print(hdr)
print("-" * len(hdr))

tc = tb = 0.0
tcc = tbb = 0.0
wins = tot = 0
worst_c = worst_b = 0.0
for c, _ in resolved:
    a, b = samples[c]
    amin, bmin = min(a), max(b) * 0 + min(b)
    amed, bmed = statistics.median(a), statistics.median(b)
    # compute-only estimate = wall - spawn floor
    ac, bc = max(amin - floor, 1e-9), max(bmin - floor, 1e-9)
    pc = (ac - bc) / bc * 100
    w = sum(1 for x, y in zip(a, b) if x < y)
    wins += w; tot += len(a)
    tc += amin; tb += bmin; tcc += ac; tbb += bc
    worst_c = max(worst_c, amin); worst_b = max(worst_b, bmin)
    flag = "*CUR" if amin < bmin else "*BEST"
    print(f"{c:<18}{amin:>8.1f}{bmin:>8.1f}{amin-bmin:>+7.1f}{pc:>+6.1f}%   {amed:>8.1f}{bmed:>8.1f}{w:>4}/{len(a)} {flag}")

print("-" * len(hdr))
print(f"{'TOTAL(wall)':<18}{tc:>8.1f}{tb:>8.1f}{tc-tb:>+7.1f}{(tc-tb)/tb*100:>+6.1f}%")
print(f"{'TOTAL(compute)':<18}{tcc:>8.1f}{tbb:>8.1f}{tcc-tbb:>+7.1f}{(tcc-tbb)/tbb*100:>+6.1f}%   (spawn floor {floor:.1f} ms removed)")
print(f"{'WORST case (LC)':<18}{worst_c:>8.1f}{worst_b:>8.1f}{worst_c-worst_b:>+7.1f}"
      f"{(worst_c-floor-(worst_b-floor))/(worst_b-floor)*100:>+6.1f}%")
print(f"CUR wins {wins}/{tot} rounds")
