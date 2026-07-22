#!/usr/bin/env python3
"""Benchmark ADD small_00 (T=200000, 1-18 digit numbers) vs best.
File redirection, 30-run min.
"""
import paramiko, sys

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

BENCH_SCRIPT = r'''
import subprocess, os, time, statistics, sys, random

RUNS = 30
WARMUP = 3

def gen_small_00():
    """LC small_00: T=200000, each number 1 to 18 digits."""
    random.seed(42)
    lines = ["200000"]
    for _ in range(200000):
        da = random.randint(1, 18)
        db = random.randint(1, 18)
        a = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(da-1)])
        b = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(db-1)])
        lines.append(f"{a} {b}")
    return '\n'.join(lines) + '\n'

def gen_1m():
    """ADD 1 million digits."""
    random.seed(42)
    a = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(999999)])
    b = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(999999)])
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

# Compile moptm ADD
print("=== Compiling moptm ADD ===")
compile_cmd = (
    "sed 's/^#define HINT_OP_DIV/\\/\\/ #define HINT_OP_DIV/' /home/azzr/moptm_fusion.cpp > /home/azzr/moptm_add_src.cpp && "
    "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD "
    "/home/azzr/moptm_add_src.cpp -o /home/azzr/moptm_add -pthread 2>&1"
)
r = subprocess.run(compile_cmd, shell=True, capture_output=True, text=True)
if r.returncode != 0:
    print(f"COMPILE FAILED:\n{r.stdout[:3000]}")
    sys.exit(1)
print("[OK] moptm_add compiled")

# Compile best ADD
if not os.path.exists('/home/azzr/best_add'):
    print("=== Compiling best ADD ===")
    r = subprocess.run(
        ['g++', '-O2', '-std=gnu++20', '-static', '-DONLINE_JUDGE',
         '/home/azzr/best/add.cpp', '-o', '/home/azzr/best_add', '-pthread'],
        capture_output=True, text=True
    )
    if r.returncode != 0:
        print(f"best COMPILE FAILED:\n{r.stderr[:3000]}")
        sys.exit(1)
    print("[OK] best_add compiled")

# Generate test inputs
print("=== Generating test inputs ===")
small_inp = gen_small_00()
with open('/tmp/bench_small_00.in', 'w') as f:
    f.write(small_inp)
print(f"[OK] small_00: {len(small_inp)} bytes, T=200000")

m1_inp = gen_1m()
with open('/tmp/bench_add_1m.in', 'w') as f:
    f.write(m1_inp)
print(f"[OK] add_1m: {len(m1_inp)} bytes")

print(f"\n=== Benchmark (file redirection, {RUNS} runs, {WARMUP} warmup, min) ===")
print(f"{'Test':<12} {'moptm_min':>10} {'moptm_med':>10} {'best_min':>10} {'best_med':>10} {'diff_min':>10}")
print("-" * 66)

for label, inp_file in [("small_00", "/tmp/bench_small_00.in"), ("add_1m", "/tmp/bench_add_1m.in")]:
    m_min, m_med = bench('/home/azzr/moptm_add', inp_file, RUNS, WARMUP)
    b_min, b_med = bench('/home/azzr/best_add', inp_file, RUNS, WARMUP)
    diff = m_min - b_min
    print(f"{label:<12} {m_min:>10.3f} {m_med:>10.3f} {b_min:>10.3f} {b_med:>10.3f} {diff:>+10.3f}")
    sys.stdout.flush()

print("\n[DONE]")
'''

import base64
BENCH_B64 = base64.b64encode(BENCH_SCRIPT.encode()).decode()

print("=== ADD small_00 Benchmark ===")
print()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded moptm_fusion.cpp")

# Check if best/add.cpp exists on VM
stdin, stdout, stderr = c.exec_command("mkdir -p /home/azzr/best && test -f /home/azzr/best/add.cpp && echo EXISTS || echo MISSING")
if stdout.read().decode().strip() == "MISSING":
    print("[INFO] Uploading best/add.cpp to VM")
    sftp = c.open_sftp()
    sftp.put("d:/precious_speed/best/add.cpp", "/home/azzr/best/add.cpp")
    sftp.close()
    print("[OK] Uploaded best/add.cpp")

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
