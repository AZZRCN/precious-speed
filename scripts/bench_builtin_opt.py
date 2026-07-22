#!/usr/bin/env python3
"""Benchmark MUL with builtin optimizations vs best (file redirection, 30-run min)

Usage: python scripts/bench_builtin_opt.py
Uploads moptm_fusion.cpp, compiles MUL mode, runs 30-run file redirection benchmark.
"""
import paramiko, sys

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

BENCH_SCRIPT = r"""
import subprocess, os, time, statistics, sys

SIZES = [
    (10000, 10000, "10k"),
    (100000, 100000, "100k"),
    (300000, 300000, "300k"),
    (500000, 500000, "500k"),
    (1000000, 1000000, "1M"),
]
RUNS = 30
WARMUP = 3

def gen_input(da, db):
    import random
    random.seed(42)
    a = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(da-1)])
    b = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(db-1)])
    return f"1\n{a} {b}\n"

def bench(exe, inp_file, runs, warmup):
    times = []
    for _ in range(warmup):
        subprocess.run(f'{exe} < {inp_file} > /dev/null', shell=True)
    for _ in range(runs):
        t0 = time.perf_counter()
        subprocess.run(f'{exe} < {inp_file} > /dev/null', shell=True)
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)
    times.sort()
    return times[0], statistics.median(times)

# Compile moptm MUL: comment out DIV define via sed, enable MUL via -D flag
print("=== Compiling moptm MUL ===")
compile_cmd = (
    "sed 's/^#define HINT_OP_DIV/\\/\\/ #define HINT_OP_DIV/' /home/azzr/moptm_fusion.cpp > /home/azzr/moptm_mul_src.cpp && "
    "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL "
    "/home/azzr/moptm_mul_src.cpp -o /home/azzr/moptm_mul -pthread 2>&1"
)
r = subprocess.run(compile_cmd, shell=True, capture_output=True, text=True)
# With 2>&1, all compiler output is in r.stdout; returncode != 0 means failure
if r.returncode != 0:
    print(f"COMPILE FAILED:\n{r.stdout[:3000]}")
    sys.exit(1)
print("[OK] moptm_mul compiled")

# Compile best MUL (if not already)
if not os.path.exists('/home/azzr/best_mul'):
    print("=== Compiling best MUL ===")
    r = subprocess.run(
        ['g++', '-O2', '-std=gnu++20', '-static', '-DONLINE_JUDGE',
         '/home/azzr/best/mul.cpp', '-o', '/home/azzr/best_mul', '-pthread'],
        capture_output=True, text=True
    )
    if r.returncode != 0:
        print(f"best COMPILE FAILED:\n{r.stderr[:3000]}")
        sys.exit(1)
    print("[OK] best_mul compiled")

print(f"\n=== Benchmark (file redirection, {RUNS} runs, {WARMUP} warmup, min) ===")
print(f"{'Size':<8} {'moptm_min':>10} {'moptm_med':>10} {'best_min':>10} {'best_med':>10} {'diff_min':>10}")
print("-" * 62)

for da, db, label in SIZES:
    inp = gen_input(da, db)
    inp_file = f'/tmp/bench_{label}.in'
    with open(inp_file, 'w') as f:
        f.write(inp)

    m_min, m_med = bench('/home/azzr/moptm_mul', inp_file, RUNS, WARMUP)
    b_min, b_med = bench('/home/azzr/best_mul', inp_file, RUNS, WARMUP)
    diff = m_min - b_min

    print(f"{label:<8} {m_min:>10.3f} {m_med:>10.3f} {b_min:>10.3f} {b_med:>10.3f} {diff:>+10.3f}")
    sys.stdout.flush()

print("\n[DONE]")
"""

import base64
BENCH_B64 = base64.b64encode(BENCH_SCRIPT.encode()).decode()

print("=== Builtin Optimization MUL Benchmark ===")
print()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded moptm_fusion.cpp")

# Check if best/mul.cpp exists on VM
stdin, stdout, stderr = c.exec_command("mkdir -p /home/azzr/best && test -f /home/azzr/best/mul.cpp && echo EXISTS || echo MISSING")
if stdout.read().decode().strip() == "MISSING":
    print("[INFO] Uploading best/mul.cpp to VM")
    sftp = c.open_sftp()
    sftp.put("d:/precious_speed/best/mul.cpp", "/home/azzr/best/mul.cpp")
    sftp.close()
    print("[OK] Uploaded best/mul.cpp")

print("\n=== Running benchmark ===")
stdin, stdout, stderr = c.exec_command(
    f"cd /home/azzr && echo {BENCH_B64} | base64 -d | python3 -u",
    timeout=600
)
out = stdout.read().decode()
print(out)
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:500]}")

c.close()
print("[DONE]")
