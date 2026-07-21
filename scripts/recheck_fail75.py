#!/usr/bin/env python3
# 重新验证 FAIL #75 + 50k/25k 异常
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

# 1. 用保存的 /tmp/fail75.in 测试
print("\n=== Test /tmp/fail75.in with both versions ===")
stdin, stdout, stderr = c.exec_command("cd /home/azzr && ./moptm_default < /tmp/fail75.in | md5sum")
d = stdout.read().decode().strip()
stdin, stdout, stderr = c.exec_command("cd /home/azzr && ./moptm_disable < /tmp/fail75.in | md5sum")
e = stdout.read().decode().strip()
print(f"default: {d}")
print(f"disable: {e}")
print(f"MATCH: {d == e}")

# 2. 重新运行 fuzz 测试 (只前 80 个, 覆盖 #75)
print("\n=== Re-run fuzz test (first 80, seed=2026) ===")
FUZZ_SCRIPT = """
import random, subprocess, hashlib
random.seed(2026)
def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n"

tests = []
for _ in range(80):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

fail = 0
for i, (a_d, b_d) in enumerate(tests):
    inp = gen(a_d, b_d)
    r1 = subprocess.run(['./moptm_default'], input=inp.encode(), capture_output=True, timeout=30)
    r2 = subprocess.run(['./moptm_disable'], input=inp.encode(), capture_output=True, timeout=30)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    if h1 != h2:
        print(f"FAIL #{i}: a={a_d} b={b_d} default={h1[:8]} (len={len(r1.stdout)}) disable={h2[:8]} (len={len(r2.stdout)}) rc={r1.returncode}")
        fail += 1
print(f"=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ===")
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3", timeout=600)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

# 3. 50k/25k benchmark 重新测试 (只这个尺寸, 50 runs)
print("\n=== 50k/25k benchmark (50 runs) ===")
BENCH_SCRIPT = """
import time, subprocess, statistics
def bench(exe, loops=50, runs=25):
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        for _ in range(loops):
            subprocess.run([exe], stdin=open('div_50k_25k.in','rb'), stdout=open('/dev/null','wb'), check=True)
        t1 = time.perf_counter()
        times.append((t1-t0)/loops*1000)
    return statistics.median(times), min(times), max(times), statistics.stdev(times)

dm,dn,dx,ds = bench('./moptm_default')
em,en,ex,es = bench('./moptm_disable')
print(f"DEFAULT: med={dm:.3f} min={dn:.3f} max={dx:.3f} stdev={ds:.3f}")
print(f"DISABLE: med={em:.3f} min={en:.3f} max={ex:.3f} stdev={es:.3f}")
print(f"diff: {(dm-em)/em*100:.2f}%")
"""
BENCH_B64 = base64.b64encode(BENCH_SCRIPT.encode()).decode()
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {BENCH_B64} | base64 -d | python3", timeout=600)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

c.close()
