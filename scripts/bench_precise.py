#!/usr/bin/env python3
"""精确 benchmark: adaptive vs old2blk, 包含 Core1 路径"""
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh

ssh.upload("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")

configs = [
    ("old2blk",  "-DDISABLE_2NXN_CYCLIC -DDIV_MU_IN_HALF"),
    ("adaptive", "-DDISABLE_2NXN_CYCLIC"),
]

print("=== Compile ===")
for name, flags in configs:
    exe = f"/home/azzr/moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} /home/azzr/moptm_fusion.cpp -o {exe} -pthread 2>&1"
    rc, out, err = ssh.run(cmd, timeout=300)
    print(f"  {name}: {'OK' if rc == 0 else 'FAIL'}")

# 测试用例: 包含 absDivMu (2:1) 和 Core1 (5:4, ~1:1)
test_cases = [
    # absDivMu 路径
    ("50k/25k",     50000,  25000),
    ("100k/50k",   100000,  50000),
    ("200k/100k",  200000, 100000),
    ("500k/250k",  500000, 250000),
    ("1M/500k",   1000000, 500000),
    # Core1 路径 (a < 2b)
    ("100k/80k",   100000,  80000),
    ("500k/400k",  500000, 400000),
    ("100k/99k",   100000,  99000),
    ("500k/499k",  500000, 499000),
]

ITERS = 10  # 10 次取最好

print(f"\n{'Config':<12} {'Test Case':<12} {'Best (ms)':<12} {'Speedup':<10}")
print("-" * 50)

for tc_name, a_d, b_d in test_cases:
    cmd = f"""python3 -c "
import random
random.seed(42)
a = ''.join([str(random.randint(0,9)) for _ in range({a_d})])
b = str(random.randint(1,9))+''.join([str(random.randint(0,9)) for _ in range({b_d}-1)])
with open('/tmp/bench.txt','w') as f:
    f.write('1\\n'+a+'\\n'+b+'\\n')
" """
    ssh.run(cmd, timeout=30)
    ssh.run("/home/azzr/moptm_old2blk < /tmp/bench.txt > /dev/null 2>&1", timeout=120)
    
    old2blk_time = None
    for name, flags in configs:
        exe = f"/home/azzr/moptm_{name}"
        cmd = f"""python3 -c "
import subprocess, time
times = []
for i in range({ITERS}):
    with open('/tmp/bench.txt') as f:
        t0 = time.perf_counter()
        subprocess.run(['{exe}'], stdin=f, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        t1 = time.perf_counter()
        times.append(t1-t0)
print(f'{{min(times)*1000:.1f}}')
" """
        rc, out, err = ssh.run(cmd, timeout=600)
        try:
            ms = float(out.strip().split('\n')[-1])
            t = ms / 1000.0
        except:
            t = None
        
        if t is not None:
            speedup = ""
            if name == "old2blk":
                old2blk_time = t
            elif old2blk_time and old2blk_time > 0:
                sp = old2blk_time / t
                speedup = f"{sp:.2f}x" + (" ↑" if sp > 1.02 else (" ↓" if sp < 0.98 else " ="))
            print(f"{name:<12} {tc_name:<12} {t:<12.3f} {speedup:<10}")
        else:
            print(f"{name:<12} {tc_name:<12} {'FAIL':<12} {'-':<10}")
    print()

print("Done!")
