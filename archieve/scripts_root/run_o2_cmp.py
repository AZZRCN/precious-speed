"""
O2 comparison: best/div.cpp VS cur div.cpp
Measures: pure div time (cur only), end-to-end time (both).
Estimates best pure div time = best_e2e - (cur_e2e - cur_pure).

Usage: python run_o2_cmp.py [size]
  size: small | medium | large | xlarge (default: xlarge)
"""
import subprocess
import sys
import time
import random
import os
from pathlib import Path

sys.set_int_max_str_digits(2000000)

ROOT = Path(r"d:\precious_speed")
BEST_EXE = ROOT / "best_div_o2.exe"
CUR_EXE = ROOT / "cur_div_o2.exe"
CUR_PURE_EXE = ROOT / "cur_div_pure_o2.exe"
TMP_IN = ROOT / "o2_cmp.in"
TMP_OUT = ROOT / "o2_cmp.out"

BASE = 10000

SIZES = {
    "small":   [(50, 50, 50), (100, 100, 20)],
    "medium":  [(500, 500, 10), (1000, 1000, 5)],
    "large":   [(5000, 5000, 3), (10000, 10000, 2)],
    "xlarge":  [(50000, 50000, 1)],
    "xxlarge": [(100000, 100000, 1)],
}


def gen_cases(b_len, q_len, n):
    cases = []
    for i in range(n):
        rng = random.Random(b_len * 10000 + q_len * 100 + i)
        b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
        if b == 0:
            b = 1
        q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
        r = rng.randint(0, b - 1)
        a = q * b + r
        cases.append((a, b))
    return cases


def write_input(cases):
    with open(TMP_IN, "w") as f:
        f.write(f"{len(cases)}\n")
        for a, b in cases:
            f.write(f"{a} {b}\n")


def run_e2e(exe_path, label):
    """Run end-to-end, return (time_ms, rc)."""
    with open(TMP_IN, "rb") as fin:
        t0 = time.perf_counter()
        try:
            r = subprocess.run([str(exe_path)], stdin=fin, capture_output=True, timeout=120)
        except subprocess.TimeoutExpired:
            print(f"  {label}: TIMEOUT", flush=True)
            return None, -1
        dt = time.perf_counter() - t0
    return dt * 1000, r.returncode


def run_pure(exe_path, label):
    """Run pure div mode, parse stderr for '[DIV] pure div time: X ms'."""
    with open(TMP_IN, "rb") as fin:
        try:
            r = subprocess.run([str(exe_path)], stdin=fin, capture_output=True, timeout=120)
        except subprocess.TimeoutExpired:
            print(f"  {label}: TIMEOUT", flush=True)
            return None, -1
    stderr = r.stderr.decode(errors="replace")
    pure_ms = None
    for line in stderr.splitlines():
        if "[DIV] pure div time:" in line:
            try:
                pure_ms = float(line.split(":")[1].strip().split()[0])
            except:
                pass
    return pure_ms, r.returncode


def verify(exe_path, cases):
    """Verify first case correctness."""
    with open(TMP_IN, "rb") as fin:
        r = subprocess.run([str(exe_path)], stdin=fin, capture_output=True, timeout=120)
    if r.returncode != 0:
        return False
    if not cases:
        return True
    a, b = cases[0]
    q, rem = divmod(a, b)
    lines = r.stdout.decode().strip().split("\n")
    if lines:
        parts = lines[0].split()
        if len(parts) >= 2:
            return parts[0] == str(q) and parts[1] == str(rem)
    return False


def main():
    size = sys.argv[1] if len(sys.argv) > 1 else "xlarge"
    if size not in SIZES:
        print(f"Unknown size: {size}")
        sys.exit(1)

    print(f"=== O2 Comparison (size={size}) ===", flush=True)
    print(f"  best: {BEST_EXE} ({BEST_EXE.stat().st_size} bytes)", flush=True)
    print(f"  cur:  {CUR_EXE} ({CUR_EXE.stat().st_size} bytes)", flush=True)
    print(f"  pure: {CUR_PURE_EXE} ({CUR_PURE_EXE.stat().st_size} bytes)", flush=True)
    print(flush=True)

    # Warm up (first run has cold-start overhead)
    warmup_cases = gen_cases(50, 50, 5)
    write_input(warmup_cases)
    run_e2e(BEST_EXE, "warmup-best")
    run_e2e(CUR_EXE, "warmup-cur")
    if CUR_PURE_EXE.exists():
        run_pure(CUR_PURE_EXE, "warmup-pure")

    print(flush=True)

    total_best_e2e = 0
    total_cur_e2e = 0
    total_cur_pure = 0

    for b_len, q_len, n in SIZES[size]:
        cases = gen_cases(b_len, q_len, n)
        write_input(cases)
        label = f"b={b_len} q={q_len} n={n}"

        # Verify correctness
        ok_best = verify(BEST_EXE, cases)
        ok_cur = verify(CUR_EXE, cases)
        if not (ok_best and ok_cur):
            print(f"  {label}: CORRECTNESS FAIL best={ok_best} cur={ok_cur}", flush=True)
            continue

        # Best end-to-end (3 runs, take min)
        best_times = []
        for _ in range(3):
            t, rc = run_e2e(BEST_EXE, f"best-{label}")
            if t is not None:
                best_times.append(t)
        best_e2e = min(best_times) if best_times else None

        # Cur end-to-end (3 runs, take min)
        cur_times = []
        for _ in range(3):
            t, rc = run_e2e(CUR_EXE, f"cur-{label}")
            if t is not None:
                cur_times.append(t)
        cur_e2e = min(cur_times) if cur_times else None

        # Cur pure div (3 runs, take min)
        cur_pure = None
        if CUR_PURE_EXE.exists():
            pure_times = []
            for _ in range(3):
                t, rc = run_pure(CUR_PURE_EXE, f"pure-{label}")
                if t is not None:
                    pure_times.append(t)
            cur_pure = min(pure_times) if pure_times else None

        # Estimate best pure div = best_e2e - (cur_e2e - cur_pure)
        io_time = (cur_e2e - cur_pure) if (cur_e2e is not None and cur_pure is not None) else None
        best_pure_est = (best_e2e - io_time) if (best_e2e is not None and io_time is not None) else None

        print(f"  {label}:", flush=True)
        print(f"    best_e2e={best_e2e:.1f}ms  cur_e2e={cur_e2e:.1f}ms  cur_pure={cur_pure:.1f}ms  io_est={io_time:.1f}ms  best_pure_est={best_pure_est:.1f}ms", flush=True)
        if cur_pure is not None and best_pure_est is not None and best_pure_est > 0:
            # delta>0: cur slower; delta<0: cur faster
            delta = cur_pure - best_pure_est
            pct = (cur_pure / best_pure_est - 1) * 100
            tag = "slower" if delta > 0 else ("faster" if delta < 0 else "equal")
            print(f"    pure_div: cur vs best = {cur_pure:.1f} vs {best_pure_est:.1f}ms  (cur {pct:+.1f}% {tag})", flush=True)
        print(flush=True)

        if best_e2e: total_best_e2e += best_e2e
        if cur_e2e: total_cur_e2e += cur_e2e
        if cur_pure: total_cur_pure += cur_pure

    print(f"=== Total (min of 3 runs each) ===", flush=True)
    print(f"  best_e2e_total={total_best_e2e:.1f}ms  cur_e2e_total={total_cur_e2e:.1f}ms  cur_pure_total={total_cur_pure:.1f}ms", flush=True)
    if total_cur_pure > 0:
        io_total = total_cur_e2e - total_cur_pure
        best_pure_total_est = total_best_e2e - io_total
        if best_pure_total_est > 0:
            delta = total_cur_pure - best_pure_total_est
            pct = (total_cur_pure / best_pure_total_est - 1) * 100
            tag = "slower" if delta > 0 else ("faster" if delta < 0 else "equal")
            print(f"  best_pure_total_est={best_pure_total_est:.1f}ms  io_total_est={io_total:.1f}ms", flush=True)
            print(f"  pure_div: cur vs best = {total_cur_pure:.1f} vs {best_pure_total_est:.1f}ms  (cur {pct:+.1f}% {tag})", flush=True)


if __name__ == "__main__":
    main()
