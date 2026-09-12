"""
Benchmark div.cpp using LC-style test cases.
Measures execution time for different input sizes.

Usage:
    python bench_div.py [size] [exe_path]
    size: tiny|small|medium|large|xlarge|mixed|all (default: medium)
    exe_path: path to div executable (default: d:\\precious_speed\\cur_div.exe)
"""
import subprocess
import sys
import time
import random
from pathlib import Path

sys.set_int_max_str_digits(2000000)

DEFAULT_DIV = Path(r"d:\precious_speed\cur_div.exe")
TMP_IN = Path(r"d:\precious_speed\div_dev\lc_test\bench.in")

BASE = 10000

# Test sizes: (name, b_len, q_len, n_cases)
SIZES = {
    "tiny":    [(10, 10, 100)],
    "small":   [(50, 50, 50), (100, 100, 20)],
    "medium":  [(500, 500, 10), (1000, 1000, 5)],
    "large":   [(5000, 5000, 3), (10000, 10000, 2)],
    "xlarge":  [(50000, 50000, 1)],
    "mixed":   [(100, 200, 10), (500, 1000, 5), (1000, 5000, 3)],
}


def gen_cases(b_len, q_len, n):
    """Generate n test cases with divisor length b_len and quotient length q_len."""
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


def run_bench(cases, label, exe_path):
    """Run exe_path on cases and measure time."""
    with open(TMP_IN, "w") as f:
        f.write(f"{len(cases)}\n")
        for a, b in cases:
            f.write(f"{a} {b}\n")

    # Warm up (first run may have overhead)
    try:
        subprocess.run([str(exe_path)], stdin=open(TMP_IN, "rb"),
                       capture_output=True, timeout=30)
    except:
        pass

    # Timed run
    t0 = time.perf_counter()
    try:
        r = subprocess.run([str(exe_path)], stdin=open(TMP_IN, "rb"),
                           capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        print(f"  {label}: TIMEOUT", flush=True)
        return None
    dt = time.perf_counter() - t0

    if r.returncode != 0:
        print(f"  {label}: FAIL rc={r.returncode}", flush=True)
        return None

    # Verify correctness (sample first case)
    if cases:
        a, b = cases[0]
        q, rem = divmod(a, b)
        lines = r.stdout.decode().strip().split("\n")
        if lines:
            parts = lines[0].split()
            if len(parts) >= 2:
                if parts[0] != str(q) or parts[1] != str(rem):
                    print(f"  {label}: WRONG ANSWER", flush=True)
                    return None

    print(f"  {label}: {dt*1000:.1f}ms ({len(cases)} cases)", flush=True)
    return dt


def main():
    size = sys.argv[1] if len(sys.argv) > 1 else "medium"
    exe_path = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_DIV

    if size == "all":
        sizes_to_run = list(SIZES.keys())
    else:
        sizes_to_run = [size]

    print(f"=== div.cpp Benchmark (size={size}) ===", flush=True)
    print(f"Binary: {exe_path}", flush=True)
    print(f"Binary size: {exe_path.stat().st_size} bytes", flush=True)
    print()

    total_time = 0
    for sz in sizes_to_run:
        if sz not in SIZES:
            print(f"Unknown size: {sz}", flush=True)
            continue
        print(f"[{sz}]", flush=True)
        for b_len, q_len, n in SIZES[sz]:
            cases = gen_cases(b_len, q_len, n)
            label = f"b_len={b_len} q_len={q_len} n={n}"
            dt = run_bench(cases, label, exe_path)
            if dt is not None:
                total_time += dt
        print()

    print(f"=== Total: {total_time*1000:.1f}ms ===", flush=True)


if __name__ == "__main__":
    main()
