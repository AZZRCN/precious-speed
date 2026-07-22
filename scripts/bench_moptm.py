#!/usr/bin/env python3
"""Benchmark moptm_fusion vs best on 6 standard test cases.
Tests: ADD 1M+1M, ADD 100k+100k, MUL 500k*500k, MUL 100k*100k, DIV 1M/500k, DIV 200k/100k
"""
import paramiko, sys, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

REMOTE_SCRIPT = r'''
import subprocess, os, sys, random, time, statistics

WORK = "/tmp/bench_moptm"
os.makedirs(WORK, exist_ok=True)

RUNS = 15
WARMUP = 3

# === Test data generation ===
def gen_num(digits):
    s = str(random.randint(1, 9))
    for _ in range(digits - 1):
        s += str(random.randint(0, 9))
    return s

def gen_add_test(name, da, db, t=1):
    random.seed(42)
    lines = [str(t)]
    for _ in range(t):
        lines.append(f"{gen_num(da)} {gen_num(db)}")
    return '\n'.join(lines) + '\n'

def gen_div_test(name, da, db, t=1):
    random.seed(42)
    lines = [str(t)]
    for _ in range(t):
        lines.append(f"{gen_num(da)} {gen_num(db)}")
    return '\n'.join(lines) + '\n'

tests = [
    # (name, mode, data)
    ("ADD_1M",     "ADD", gen_add_test("ADD_1M", 1000000, 1000000)),
    ("ADD_100k",   "ADD", gen_add_test("ADD_100k", 100000, 100000)),
    ("MUL_500k",   "MUL", gen_add_test("MUL_500k", 500000, 500000)),
    ("MUL_100k",   "MUL", gen_add_test("MUL_100k", 100000, 100000)),
    ("DIV_1M_500k","DIV", gen_div_test("DIV_1M_500k", 1000000, 500000)),
    ("DIV_200k_100k","DIV", gen_div_test("DIV_200k_100k", 200000, 100000)),
]

# Write test files
for name, mode, data in tests:
    with open(f"{WORK}/{name}.in", 'w') as f:
        f.write(data)
    print(f"[OK] {name}: {len(data)} bytes")

# === Binary paths ===
moptm_exes = {
    "ADD": f"{WORK}/moptm_ADD",
    "MUL": f"{WORK}/moptm_MUL",
    "DIV": f"{WORK}/moptm_DIV",
}
best_exes = {
    "ADD": "/home/azzr/best_add",
    "MUL": "/home/azzr/best_mul",
    "DIV": "/home/azzr/best_div",
}

# Check binaries exist
for mode, exe in moptm_exes.items():
    if not os.path.exists(exe):
        print(f"[FAIL] {exe} not found")
        sys.exit(1)

# Compile best if missing
for mode, exe in best_exes.items():
    if not os.path.exists(exe):
        src = f"/home/azzr/best/{mode.lower()}.cpp"
        if not os.path.exists(src):
            print(f"[WARN] {src} not found, skipping best for {mode}")
            best_exes[mode] = None
            continue
        r = subprocess.run(
            f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE {src} -o {exe} -pthread 2>&1",
            shell=True, capture_output=True, text=True
        )
        if r.returncode != 0:
            print(f"[FAIL] compile best {mode}: {r.stdout[:1000]}")
            best_exes[mode] = None
        else:
            print(f"[OK] compiled best_{mode.lower()}")

# === Benchmark function ===
def bench(exe, inp_file, runs, warmup):
    for _ in range(warmup):
        subprocess.run(f'{exe} < {inp_file} > /dev/null', shell=True)
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        subprocess.run(f'{exe} < {inp_file} > /dev/null', shell=True)
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)
    times.sort()
    return times[0], statistics.median(times)

# === Run benchmarks ===
print(f"\n=== Benchmark ({RUNS} runs, {WARMUP} warmup, min/median ms) ===")
print(f"{'Test':<16} {'moptm_min':>10} {'moptm_med':>10} {'best_min':>10} {'best_med':>10} {'diff_min':>10}")
print("-" * 70)
sys.stdout.flush()

for name, mode, data in tests:
    inp_file = f"{WORK}/{name}.in"
    m_exe = moptm_exes[mode]
    b_exe = best_exes.get(mode)

    m_min, m_med = bench(m_exe, inp_file, RUNS, WARMUP)
    if b_exe:
        b_min, b_med = bench(b_exe, inp_file, RUNS, WARMUP)
        diff = m_min - b_min
        pct = (diff / b_min) * 100
        print(f"{name:<16} {m_min:>10.3f} {m_med:>10.3f} {b_min:>10.3f} {b_med:>10.3f} {diff:>+10.3f} ({pct:+.1f}%)")
    else:
        print(f"{name:<16} {m_min:>10.3f} {m_med:>10.3f} {'N/A':>10} {'N/A':>10} {'N/A':>10}")
    sys.stdout.flush()

print("\n[DONE]")
'''

B64 = base64.b64encode(REMOTE_SCRIPT.encode()).decode()

print("=== moptm_fusion Benchmark ===")
print()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected to {HOST}")

print("\n=== Running benchmark ===")
stdin, stdout, stderr = c.exec_command(
    f"cd /home/azzr && echo {B64} | base64 -d | python3 -u",
    timeout=600
)
out = stdout.read().decode()
print(out)
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:500]}")

c.close()
print("[DONE]")
