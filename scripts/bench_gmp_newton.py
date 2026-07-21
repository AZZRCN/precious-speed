#!/usr/bin/env python3
# VM 独占下重新验证 absInvNewtonGMP cyclic 路径性能
# baseline (absInvNewton) vs USE_GMP_NEWTON (absInvNewtonGMP cyclic)
import sys
import paramiko
import time

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

CMD_COMPILE_BASELINE = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_fusion.cpp -o moptm_base -pthread"
CMD_COMPILE_GMP = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DUSE_GMP_NEWTON moptm_fusion.cpp -o moptm_gmp -pthread"
CMD_MD5_BASE = "./moptm_base < div_1M_500k.in | md5sum"
CMD_MD5_GMP = "./moptm_gmp < div_1M_500k.in | md5sum"
CMD_MD5_BASE_2 = "./moptm_base < div_200k_100k.in | md5sum"
CMD_MD5_GMP_2 = "./moptm_gmp < div_200k_100k.in | md5sum"

# benchmark: 15 runs × 50 loops, 取中位数
import base64
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
    print("\n=== Step 2: Compile baseline ===")
    rc, out, err = run(c, CMD_COMPILE_BASELINE, timeout=300)
    print(f"[EXIT {rc}]")
    if err: print(f"[STDERR] {err}")
    if rc != 0:
        print("[FAIL] baseline compile failed")
        return

    print("\n=== Step 3: Compile USE_GMP_NEWTON ===")
    rc, out, err = run(c, CMD_COMPILE_GMP, timeout=300)
    print(f"[EXIT {rc}]")
    if err: print(f"[STDERR] {err}")
    if rc != 0:
        print("[FAIL] GMP compile failed")
        return

    # 3. MD5 正确性验证
    print("\n=== Step 4: MD5 correctness check ===")
    rc, b1, _ = run(c, CMD_MD5_BASE)
    rc, g1, _ = run(c, CMD_MD5_GMP)
    rc, b2, _ = run(c, CMD_MD5_BASE_2)
    rc, g2, _ = run(c, CMD_MD5_GMP_2)
    print(f"1M/500k    BASE: {b1.strip()}")
    print(f"1M/500k    GMP : {g1.strip()}")
    print(f"200k/100k  BASE: {b2.strip()}")
    print(f"200k/100k  GMP : {g2.strip()}")
    match1 = b1.strip() == g1.strip()
    match2 = b2.strip() == g2.strip()
    print(f"1M/500k    MATCH: {match1}")
    print(f"200k/100k MATCH: {match2}")
    if not (match1 and match2):
        print("[FAIL] MD5 mismatch, abort benchmark")
        return

    # 4. Benchmark
    print("\n=== Step 5: Benchmark (15 runs × 50 loops, median) ===")
    rc, out, err = run(c, f"echo {BENCH_B64} | base64 -d | python3", timeout=1800)
    print(f"[EXIT {rc}]")
    print(out)
    if err: print(f"[STDERR] {err}")

    c.close()
    print("\n[Done]")

if __name__ == "__main__":
    main()
