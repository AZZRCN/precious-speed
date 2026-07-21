#!/usr/bin/env python3
# 全面 benchmark: BASE vs GMP cyclic, 多尺寸, 多次运行
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# 测试多个尺寸: 生成测试数据 + benchmark
SETUP_SCRIPT = """
cd /home/azzr
# 生成多尺寸测试数据 (如果不存在)
python3 -c "
import random
random.seed(42)
sizes = [(50000,25000), (100000,50000), (200000,100000), (500000,250000), (1000000,500000)]
for n,d in sizes:
    fname = f'div_{n//1000}k_{d//1000}k.in'
    try:
        with open(fname,'r') as f: f.read(1)
        print(f'{fname} exists')
    except:
        a = ''.join([str(random.randint(0,9)) for _ in range(n)])
        b = ''.join([str(random.randint(0,9)) for _ in range(d)])
        with open(fname,'w') as f: f.write(f'1\\n{a}\\n{b}\\n')
        print(f'{fname} generated')
"
ls -la div_*.in
"""

BENCH_TEMPLATE = """
import time, subprocess, statistics, sys
def bench(exe, inp, loops=30, runs=21):
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        for _ in range(loops):
            subprocess.run([exe], stdin=open(inp,'rb'), stdout=open('/dev/null','wb'), check=True)
        t1 = time.perf_counter()
        times.append((t1-t0)/loops*1000)
    return statistics.median(times), min(times), max(times), statistics.stdev(times)

tests = ['div_50k_25k.in','div_100k_50k.in','div_200k_100k.in','div_500k_250k.in','div_1000k_500k.in']
print(f"{'Test':<22} {'BASE med':>10} {'GMP med':>10} {'diff':>8} {'BASE stdev':>12} {'GMP stdev':>12}")
for inp in tests:
    try:
        bm,bn,bx,bs = bench('./moptm_base', inp)
        gm,gn,gx,gs = bench('./moptm_gmp', inp)
        diff = (gm-bm)/bm*100
        print(f"{inp:<22} {bm:>10.3f} {gm:>10.3f} {diff:>7.2f}% {bs:>12.3f} {gs:>12.3f}")
    except Exception as e:
        print(f"{inp:<22} ERROR: {e}")
"""
BENCH_B64 = base64.b64encode(BENCH_TEMPLATE.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected to {USER}@{HOST}")

print("\n=== Step 1: Setup test data ===")
stdin, stdout, stderr = c.exec_command(SETUP_SCRIPT, timeout=120)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

print("\n=== Step 2: Full Benchmark (21 runs × 30 loops) ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {BENCH_B64} | base64 -d | python3", timeout=3600)
out = stdout.read().decode()
err = stderr.read().decode()
rc = stdout.channel.recv_exit_status()
print(f"[EXIT {rc}]")
print(out)
if err: print(f"[STDERR] {err}")

c.close()
