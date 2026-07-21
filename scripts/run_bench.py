#!/usr/bin/env python3
# 直接在 VM 上运行 benchmark (二进制已编译)
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

BENCH_TEMPLATE = """
import time, subprocess, statistics
def bench(exe, inp, loops=50, runs=15):
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        for _ in range(loops):
            subprocess.run([exe], stdin=open(inp,'rb'), stdout=open('/dev/null','wb'), check=True)
        t1 = time.perf_counter()
        times.append((t1-t0)/loops*1000)
    return statistics.median(times), min(times), max(times)

for exe, label in [('./moptm_base','BASE'), ('./moptm_gmp','GMP ')]:
    for inp in ['div_1M_500k.in','div_200k_100k.in']:
        med, mn, mx = bench(exe, inp)
        print(f"{label} {inp}: med={med:.3f}ms min={mn:.3f}ms max={mx:.3f}ms")
"""
BENCH_B64 = base64.b64encode(BENCH_TEMPLATE.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected to {USER}@{HOST}")

# 检查二进制是否存在
stdin, stdout, stderr = c.exec_command("ls -la moptm_base moptm_gmp div_*.in")
print(stdout.read().decode())

print("\n=== Benchmark (15 runs × 50 loops, median) ===")
stdin, stdout, stderr = c.exec_command(f"echo {BENCH_B64} | base64 -d | python3", timeout=1800)
out = stdout.read().decode()
err = stderr.read().decode()
rc = stdout.channel.recv_exit_status()
print(f"[EXIT {rc}]")
print(out)
if err: print(f"[STDERR] {err}")

c.close()
