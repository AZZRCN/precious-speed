#!/usr/bin/env python3
"""VM benchmark: precise timing with perf_counter."""
import subprocess
import time
import os

os.chdir(os.path.expanduser("~/div_bench"))

CXXFLAGS = "-std=c++20 -O2 -march=native -DEVAL -DONLINE_JUDGE -I."
LDFLAGS = "-lpthread"

# (exe_name, src, label)
EXES = [
    ("best_add", "best_add.cpp"),
    ("cur_add",  "cur_add.cpp"),
    ("best_mul", "best_mul.cpp"),
    ("cur_mul",  "cur_mul.cpp"),
    ("best_div", "best_div.cpp"),
    ("cur_div",  "cur_div.cpp"),
]

# (data_name, input_file)
TESTS = {
    "add": [("add_max_0", "add_max_0.in")],
    "mul": [("mul_max_0", "mul_max_0.in")],
    "div": [
        ("div_max_0",   "div_max_0.in"),
        ("div_max_2",   "div_max_2.in"),
        ("div_medium",  "div_medium_0.in"),
        ("div_large",   "div_large_0.in"),
    ],
}

def run_bench(exe, input_file, runs=5):
    """Run benchmark, return min time in ms."""
    with open(input_file, "rb") as f:
        data = f.read()
    # Warm up
    try:
        subprocess.run([f"./{exe}"], input=data, capture_output=True, timeout=60)
    except:
        pass
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        try:
            r = subprocess.run([f"./{exe}"], input=data, capture_output=True, timeout=60)
        except subprocess.TimeoutExpired:
            return None, "TIMEOUT"
        dt = (time.perf_counter() - t0) * 1000
        if r.returncode != 0:
            return None, f"RC={r.returncode}"
        times.append(dt)
    return min(times), None

print("=" * 70)
print("VM Benchmark: best VS cur (O2 -march=native, LC flags)")
print("=" * 70)
print(f"CPU: {os.cpu_count()} cores")
print(f"GCC: {subprocess.check_output(['g++', '--version']).decode().splitlines()[0]}")
print()

for op in ["add", "mul", "div"]:
    print(f"--- {op.upper()} ---")
    for label, input_file in TESTS[op]:
        if not os.path.exists(input_file):
            print(f"  {label}: SKIP (no input)")
            continue
        size = os.path.getsize(input_file)
        results = {}
        for exe_name, _ in EXES:
            if not exe_name.startswith(op.split("_")[0]) and not exe_name.endswith(f"_{op}"):
                # Match best_add/cur_add for add, best_mul/cur_mul for mul, best_div/cur_div for div
                if not (exe_name.endswith(f"_{op}") or exe_name.endswith(f"_{op}")):
                    continue
            # Simple match: exe_name contains op
            if op not in exe_name:
                continue
            if not os.path.exists(f"./{exe_name}"):
                continue
            t, err = run_bench(exe_name, input_file, runs=5)
            if err:
                results[exe_name] = (None, err)
                print(f"  {exe_name:12s} {label:15s}: {err}")
            else:
                results[exe_name] = (t, None)
                print(f"  {exe_name:12s} {label:15s}: {t:.1f}ms")
        
        # Compare
        best_key = f"best_{op}"
        cur_key = f"cur_{op}"
        if best_key in results and cur_key in results:
            bt = results[best_key][0]
            ct = results[cur_key][0]
            if bt and ct and bt > 0:
                diff = (ct / bt - 1) * 100
                tag = "slower" if diff > 0 else ("faster" if diff < 0 else "equal")
                print(f"  -> cur vs best: {ct:.1f} vs {bt:.1f}ms ({diff:+.1f}% {tag})")
        print()

print("=" * 70)
print("Done.")
