#!/usr/bin/env python3
# 验证默认启用 GMP cyclic 的正确性和性能
# 1. 上传修改后源码
# 2. 编译默认版本 (GMP cyclic 启用) 和 DISABLE_GMP_NEWTON 版本
# 3. MD5 正确性验证 (多尺寸)
# 4. 大规模 fuzz 测试 (100 组随机尺寸)
# 5. benchmark 对比
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

CMD_COMPILE_DEFAULT = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_fusion.cpp -o moptm_default -pthread"
CMD_COMPILE_DISABLE = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DDISABLE_GMP_NEWTON moptm_fusion.cpp -o moptm_disable -pthread"

# Fuzz 测试: 100 组随机尺寸
FUZZ_SCRIPT = """
import random, subprocess, hashlib
random.seed(2026)
def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n"

# 100 组随机尺寸 (覆盖小/中/大)
tests = []
for _ in range(100):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    # 确保 b 位数是 4 的倍数 (BASE=10^4 要求)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

fail = 0
for i, (a_d, b_d) in enumerate(tests):
    inp = gen(a_d, b_d)
    r1 = subprocess.run(['./moptm_default'], input=inp.encode(), capture_output=True)
    r2 = subprocess.run(['./moptm_disable'], input=inp.encode(), capture_output=True)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    if h1 != h2:
        print(f"FAIL #{i}: a={a_d} b={b_d} default={h1[:8]} disable={h2[:8]}")
        fail += 1
    elif i % 20 == 0:
        print(f"OK #{i}: a={a_d} b={b_d}")
print(f"=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ===")
"""

BENCH_SCRIPT = """
import time, subprocess, statistics
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
print(f"{'Test':<22} {'DEFAULT':>10} {'DISABLE':>10} {'diff':>8} {'DEF stdev':>10} {'DIS stdev':>10}")
for inp in tests:
    try:
        dm,dn,dx,ds = bench('./moptm_default', inp)
        em,en,ex,es = bench('./moptm_disable', inp)
        diff = (dm-em)/em*100
        print(f"{inp:<22} {dm:>10.3f} {em:>10.3f} {diff:>7.2f}% {ds:>10.3f} {es:>10.3f}")
    except Exception as e:
        print(f"{inp:<22} ERROR: {e}")
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()
BENCH_B64 = base64.b64encode(BENCH_SCRIPT.encode()).decode()

def get_client():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PWD, timeout=10)
    return c

def run(c, cmd, timeout=600):
    stdin, stdout, stderr = c.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

def upload(c, local, remote):
    sftp = c.open_sftp()
    sftp.put(local, remote)
    sftp.close()

def main():
    c = get_client()
    print(f"[OK] SSH connected to {USER}@{HOST}")

    # 1. 上传最新源码
    print("\n=== Step 1: Upload moptm_fusion.cpp ===")
    upload(c, "d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
    print("[OK] Uploaded")

    # 2. 编译
    print("\n=== Step 2: Compile default (GMP cyclic enabled) ===")
    rc, out, err = run(c, CMD_COMPILE_DEFAULT, timeout=300)
    print(f"[EXIT {rc}]")
    if err: print(f"[STDERR] {err}")
    if rc != 0:
        print("[FAIL] default compile failed")
        return

    print("\n=== Step 3: Compile DISABLE_GMP_NEWTON (fallback) ===")
    rc, out, err = run(c, CMD_COMPILE_DISABLE, timeout=300)
    print(f"[EXIT {rc}]")
    if err: print(f"[STDERR] {err}")
    if rc != 0:
        print("[FAIL] disable compile failed")
        return

    # 3. MD5 正确性验证
    print("\n=== Step 4: MD5 correctness check (multi-size) ===")
    for inp in ['div_50k_25k.in','div_100k_50k.in','div_200k_100k.in','div_500k_250k.in','div_1000k_500k.in']:
        rc, d, _ = run(c, f"./moptm_default < {inp} | md5sum")
        rc, e, _ = run(c, f"./moptm_disable < {inp} | md5sum")
        match = d.strip() == e.strip()
        print(f"{inp:<22} default={d.strip()[:16]} disable={e.strip()[:16]} {'MATCH' if match else 'MISMATCH!'}")

    # 4. Fuzz 测试
    print("\n=== Step 5: Fuzz test (100 random sizes) ===")
    rc, out, err = run(c, f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3", timeout=600)
    print(f"[EXIT {rc}]")
    print(out)
    if err: print(f"[STDERR] {err}")

    # 5. Benchmark
    print("\n=== Step 6: Benchmark (21 runs × 30 loops) ===")
    rc, out, err = run(c, f"cd /home/azzr && echo {BENCH_B64} | base64 -d | python3", timeout=3600)
    print(f"[EXIT {rc}]")
    print(out)
    if err: print(f"[STDERR] {err}")

    c.close()
    print("\n[Done]")

if __name__ == "__main__":
    main()
