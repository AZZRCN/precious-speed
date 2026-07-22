#!/usr/bin/env python3
"""Benchmark fast_io.cpp: compare 3 parse versions × 2 write versions.
Uploads fast_io.cpp, compiles 6 variants, generates small_00 test data,
verifies correctness vs best/add.cpp, and benchmarks 30 runs.
"""
import paramiko, sys, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

BENCH_SCRIPT = r'''
import subprocess, os, time, statistics, sys, random, hashlib

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

# Generate test input
print("=== Generating small_00 test data ===")
small_inp = gen_small_00()
with open('/tmp/bench_small_00.in', 'w') as f:
    f.write(small_inp)
print(f"[OK] small_00: {len(small_inp)} bytes, T=200000")

# Compile best ADD (reference)
if not os.path.exists('/home/azzr/best_add'):
    print("=== Compiling best ADD ===")
    os.makedirs('/home/azzr/best', exist_ok=True)
    r = subprocess.run(
        ['g++', '-O2', '-std=gnu++20', '-static', '-DONLINE_JUDGE',
         '/home/azzr/best/add.cpp', '-o', '/home/azzr/best_add', '-pthread'],
        capture_output=True, text=True
    )
    if r.returncode != 0:
        print(f"best COMPILE FAILED:\n{r.stderr[:3000]}")
        sys.exit(1)
    print("[OK] best_add compiled")

# Compile fast_io variants
variants = []
for pv in [1, 4]:
    for wv in [1, 2]:
        name = f"fast_io_p{pv}w{wv}"
        src = f"/home/azzr/{name}.cpp"
        exe = f"/home/azzr/{name}"
        # Create variant source with -D flags
        r = subprocess.run(
            f'g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE '
            f'-DPARSE_VER={pv} -DWRITE_VER={wv} '
            f'/home/azzr/fast_io.cpp -o {exe} -pthread 2>&1',
            shell=True, capture_output=True, text=True
        )
        if r.returncode != 0:
            print(f"[FAIL] {name} compile:\n{r.stdout[:2000]}")
            continue
        variants.append((name, exe))
        print(f"[OK] {name} compiled")

if not variants:
    print("[ERROR] No variants compiled successfully")
    sys.exit(1)

# Correctness check: compare each variant vs best
print("\n=== Correctness check (vs best_add) ===")
subprocess.run(f'/home/azzr/best_add < /tmp/bench_small_00.in > /tmp/best_out.txt', shell=True, check=True)
with open('/tmp/best_out.txt', 'rb') as f:
    best_md5 = hashlib.md5(f.read()).hexdigest()
print(f"  best: {best_md5[:12]}")

all_pass = True
for name, exe in variants:
    r = subprocess.run(f'{exe} < /tmp/bench_small_00.in > /tmp/var_out.txt', shell=True)
    if r.returncode != 0:
        print(f"  {name}: RUNTIME ERROR (exit {r.returncode})")
        all_pass = False
        continue
    with open('/tmp/var_out.txt', 'rb') as f:
        var_md5 = hashlib.md5(f.read()).hexdigest()
    ok = "PASS" if var_md5 == best_md5 else "FAIL"
    if var_md5 != best_md5:
        all_pass = False
    print(f"  {name}: {var_md5[:12]} [{ok}]")

if not all_pass:
    print("\n[ERROR] Correctness check failed! Aborting benchmark.")
    sys.exit(1)

# Benchmark
print(f"\n=== Benchmark (file redirection, {RUNS} runs, {WARMUP} warmup, min) ===")
print(f"{'Variant':<16} {'min_ms':>10} {'med_ms':>10} {'vs_best':>10}")
print("-" * 50)

# Benchmark best first
b_min, b_med = bench('/home/azzr/best_add', '/tmp/bench_small_00.in', RUNS, WARMUP)
print(f"{'best_add':<16} {b_min:>10.3f} {b_med:>10.3f} {'(base)':>10}")
sys.stdout.flush()

for name, exe in variants:
    m_min, m_med = bench(exe, '/tmp/bench_small_00.in', RUNS, WARMUP)
    diff = m_min - b_min
    print(f"{name:<16} {m_min:>10.3f} {m_med:>10.3f} {diff:>+10.3f}")
    sys.stdout.flush()

print("\n[DONE]")
'''

BENCH_B64 = base64.b64encode(BENCH_SCRIPT.encode()).decode()

print("=== fast_io Benchmark ===")
print()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected to {HOST}")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/fast_io.cpp", "/home/azzr/fast_io.cpp")
sftp.close()
print("[OK] Uploaded fast_io.cpp")

# Check if best/add.cpp exists
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
