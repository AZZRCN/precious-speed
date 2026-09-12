"""
Full benchmark: best VS cur for ADD/MUL/DIV under O2 and O3.
Uses LC generators from E:\\library-checker-problems-master\\big_integer.

Steps:
1. Compile LC generators (add/mul/div × max/medium/large)
2. Generate test data
3. Compile 12 executables (best/cur × add/mul/div × O2/O3)
4. Run benchmarks (3 runs each, take min)
5. Print summary table

Usage: python run_full_bench.py
"""
import subprocess
import sys
import time
import os
from pathlib import Path

sys.set_int_max_str_digits(5000000)

# === Paths ===
ROOT = Path(r"d:\precious_speed")
TTO = ROOT / "toolbox" / "tto.exe"
LC_ROOT = Path(r"E:\library-checker-problems-master\big_integer")
LC_COMMON = Path(r"E:\library-checker-problems-master\common")
SHIM = ROOT / "archieve" / "win_memalign_shim.h"
GEN_DIR = ROOT / "gen_bin"
DATA_DIR = ROOT / "bench_data"
BIN_DIR = ROOT / "bench_bin"

# === Config ===
O2FLAGS = ["-std=c++20", "-O2", "-I."]
O3FLAGS = ["-std=c++20", "-O3", "-I."]
LDFLAGS = ["-lpthread"]

# LC generators to compile: (name, problem_dir, gen_file, seeds)
GENERATORS = [
    # ADD
    ("gen_add_max",     "addition_of_big_integers",       "max_max.cpp", [0, 1, 2, 3]),
    ("gen_add_medium",  "addition_of_big_integers",       "medium.cpp",  [0, 1, 2]),
    ("gen_add_large",   "addition_of_big_integers",       "large.cpp",   [0, 1]),
    # MUL
    ("gen_mul_max",     "multiplication_of_big_integers", "max_max.cpp", [0, 1, 2, 3]),
    ("gen_mul_medium",  "multiplication_of_big_integers", "medium.cpp",  [0, 1, 2]),
    ("gen_mul_large",   "multiplication_of_big_integers", "large.cpp",   [0, 1]),
    # DIV
    ("gen_div_max",     "division_of_big_integers",       "max.cpp",     [0, 1, 2]),
    ("gen_div_medium",  "division_of_big_integers",       "medium.cpp",  [0, 1, 2]),
    ("gen_div_large",   "division_of_big_integers",       "large.cpp",   [0, 1]),
]

# Test data: (data_name, gen_name, seed)
TEST_DATA = [
    # ADD
    ("add_max_0",    "gen_add_max",    0),  # 2M+2M digits, 1 case
    ("add_max_1",    "gen_add_max",    1),
    ("add_medium_0", "gen_add_medium", 0),  # ~1k digits, many cases
    ("add_medium_1", "gen_add_medium", 1),
    ("add_large_0",  "gen_add_large",  0),  # ~100k digits, several cases
    # MUL
    ("mul_max_0",    "gen_mul_max",    0),  # 2M×2M digits, 1 case
    ("mul_max_1",    "gen_mul_max",    1),
    ("mul_medium_0", "gen_mul_medium", 0),
    ("mul_large_0",  "gen_mul_large",  0),
    # DIV
    ("div_max_0",    "gen_div_max",    0),  # 2M/2M digits, 1 case
    ("div_max_2",    "gen_div_max",    2),  # A=MAX-1, B=MAX
    ("div_medium_0", "gen_div_medium", 0),
    ("div_large_0",  "gen_div_large",  0),
]

# Executables: (name, src, flags, needs_shim)
EXES = [
    # best
    ("best_add_O2", ROOT / "best" / "add.cpp", O2FLAGS, True),
    ("best_add_O3", ROOT / "best" / "add.cpp", O3FLAGS, True),
    ("best_mul_O2", ROOT / "best" / "mul.cpp", O2FLAGS, True),
    ("best_mul_O3", ROOT / "best" / "mul.cpp", O3FLAGS, True),
    ("best_div_O2", ROOT / "best" / "div.cpp", O2FLAGS, True),
    ("best_div_O3", ROOT / "best" / "div.cpp", O3FLAGS, True),
    # cur
    ("cur_add_O2", ROOT / "add.cpp", O2FLAGS, False),
    ("cur_add_O3", ROOT / "add.cpp", O3FLAGS, False),
    ("cur_mul_O2", ROOT / "mul.cpp", O2FLAGS, False),
    ("cur_mul_O3", ROOT / "mul.cpp", O3FLAGS, False),
    ("cur_div_O2", ROOT / "div.cpp", O2FLAGS, False),
    ("cur_div_O3", ROOT / "div.cpp", O3FLAGS, False),
]

# Mapping: operation -> which test data
OP_TESTS = {
    "add": ["add_max_0", "add_max_1", "add_medium_0", "add_medium_1", "add_large_0"],
    "mul": ["mul_max_0", "mul_max_1", "mul_medium_0", "mul_large_0"],
    "div": ["div_max_0", "div_max_2", "div_medium_0", "div_large_0"],
}


def run(cmd, timeout=60, cwd=None):
    """Run command with timeout, return (rc, stdout, stderr)."""
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=timeout, cwd=cwd)
        return r.returncode, r.stdout.decode(errors="replace"), r.stderr.decode(errors="replace")
    except subprocess.TimeoutExpired:
        return 124, "", "TIMEOUT"
    except Exception as e:
        return 1, "", str(e)


def compile_gen(name, problem_dir, gen_file):
    """Compile an LC generator."""
    src = LC_ROOT / problem_dir / "gen" / gen_file
    out = GEN_DIR / f"{name}.exe"
    cmd = ["g++", "-std=c++20", "-O2",
           "-I", str(LC_COMMON),
           "-I", str(LC_ROOT / problem_dir),
           str(src), "-o", str(out)]
    rc, _, err = run(cmd, timeout=30)
    if rc != 0:
        print(f"  [FAIL] {name}: {err[:200]}", flush=True)
        return False
    return True


def gen_data(name, gen_name, seed):
    """Generate test data by running generator."""
    gen_exe = GEN_DIR / f"{gen_name}.exe"
    out_file = DATA_DIR / f"{name}.in"
    cmd = [str(gen_exe), str(seed)]
    rc, out, err = run(cmd, timeout=60)
    if rc != 0:
        print(f"  [FAIL] gen {name}: rc={rc} {err[:200]}", flush=True)
        return False
    out_file.write_text(out)
    return True


def compile_exe(name, src, flags, needs_shim):
    """Compile a benchmark executable."""
    out = BIN_DIR / f"{name}.exe"
    cmd = ["g++"] + flags
    if needs_shim:
        cmd += ["-include", str(SHIM)]
    cmd += [str(src), "-o", str(out)] + LDFLAGS
    rc, _, err = run(cmd, timeout=120)
    if rc != 0:
        print(f"  [FAIL] {name}: {err[:300]}", flush=True)
        return False
    return True


def bench(exe_path, input_path, runs=3, timeout=120):
    """Run benchmark, return min time in ms."""
    data = input_path.read_bytes()
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        try:
            r = subprocess.run([str(exe_path)], input=data, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            return None, "TIMEOUT"
        dt = (time.perf_counter() - t0) * 1000
        if r.returncode != 0:
            return None, f"RC={r.returncode}"
        times.append(dt)
    return min(times), None


def verify(exe_path, input_path, timeout=60):
    """Quick sanity: run and check rc=0."""
    data = input_path.read_bytes()
    try:
        r = subprocess.run([str(exe_path)], input=data, capture_output=True, timeout=timeout)
        return r.returncode == 0
    except:
        return False


def main():
    # Create dirs
    for d in [GEN_DIR, DATA_DIR, BIN_DIR]:
        d.mkdir(exist_ok=True)

    # === Step 1: Compile generators ===
    print("=" * 60, flush=True)
    print("Step 1: Compile LC generators", flush=True)
    print("=" * 60, flush=True)
    gen_ok = 0
    for name, problem_dir, gen_file, _ in GENERATORS:
        if compile_gen(name, problem_dir, gen_file):
            gen_ok += 1
    print(f"  {gen_ok}/{len(GENERATORS)} generators compiled", flush=True)
    print(flush=True)

    # === Step 2: Generate test data ===
    print("=" * 60, flush=True)
    print("Step 2: Generate test data", flush=True)
    print("=" * 60, flush=True)
    data_ok = 0
    for name, gen_name, seed in TEST_DATA:
        if gen_data(name, gen_name, seed):
            size = (DATA_DIR / f"{name}.in").stat().st_size
            print(f"  {name}: {size} bytes", flush=True)
            data_ok += 1
    print(f"  {data_ok}/{len(TEST_DATA)} data files generated", flush=True)
    print(flush=True)

    # === Step 3: Compile executables ===
    print("=" * 60, flush=True)
    print("Step 3: Compile 12 executables (best/cur × add/mul/div × O2/O3)", flush=True)
    print("=" * 60, flush=True)
    exe_ok = 0
    for name, src, flags, needs_shim in EXES:
        print(f"  Compiling {name}...", flush=True, end=" ")
        if compile_exe(name, src, flags, needs_shim):
            print("OK", flush=True)
            exe_ok += 1
        else:
            print("FAIL", flush=True)
    print(f"  {exe_ok}/{len(EXES)} executables compiled", flush=True)
    print(flush=True)

    if exe_ok < len(EXES):
        print("WARNING: not all executables compiled, continuing with available ones", flush=True)
        print(flush=True)

    # === Step 4: Run benchmarks ===
    print("=" * 60, flush=True)
    print("Step 4: Run benchmarks (3 runs each, min)", flush=True)
    print("=" * 60, flush=True)

    results = {}  # (exe_name, data_name) -> (time_ms, error)

    for op in ["add", "mul", "div"]:
        print(f"\n--- {op.upper()} ---", flush=True)
        for data_name in OP_TESTS[op]:
            input_path = DATA_DIR / f"{data_name}.in"
            if not input_path.exists():
                print(f"  {data_name}: SKIP (no data)", flush=True)
                continue
            size = input_path.stat().st_size
            print(f"  {data_name} ({size} bytes):", flush=True)

            for opt in ["O2", "O3"]:
                for version in ["best", "cur"]:
                    exe_name = f"{version}_{op}_{opt}"
                    exe_path = BIN_DIR / f"{exe_name}.exe"
                    if not exe_path.exists():
                        print(f"    {exe_name}: SKIP (no exe)", flush=True)
                        continue
                    t, err = bench(exe_path, input_path, runs=3, timeout=180)
                    if err:
                        print(f"    {exe_name}: {err}", flush=True)
                        results[(exe_name, data_name)] = (None, err)
                    else:
                        print(f"    {exe_name}: {t:.1f}ms", flush=True)
                        results[(exe_name, data_name)] = (t, None)

    # === Step 5: Summary ===
    print(flush=True)
    print("=" * 60, flush=True)
    print("Step 5: Summary", flush=True)
    print("=" * 60, flush=True)

    for op in ["add", "mul", "div"]:
        print(f"\n=== {op.upper()} ===", flush=True)
        header = f"  {'data':<20} {'best_O2':>10} {'cur_O2':>10} {'O2_diff':>10} {'best_O3':>10} {'cur_O3':>10} {'O3_diff':>10}"
        print(header, flush=True)
        print("  " + "-" * (len(header) - 2), flush=True)
        for data_name in OP_TESTS[op]:
            row = [f"  {data_name:<20}"]
            for opt in ["O2", "O3"]:
                best_t = results.get((f"best_{op}_{opt}", data_name), (None, None))[0]
                cur_t = results.get((f"cur_{op}_{opt}", data_name), (None, None))[0]
                if best_t is not None:
                    row.append(f"{best_t:>10.1f}")
                else:
                    row.append(f"{'N/A':>10}")
                if cur_t is not None:
                    row.append(f"{cur_t:>10.1f}")
                else:
                    row.append(f"{'N/A':>10}")
                if best_t is not None and cur_t is not None and best_t > 0:
                    diff = (cur_t / best_t - 1) * 100
                    row.append(f"{diff:>+9.1f}%")
                else:
                    row.append(f"{'N/A':>10}")
            print(" ".join(row), flush=True)


if __name__ == "__main__":
    main()
