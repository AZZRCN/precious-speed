#!/usr/bin/env python3
"""对比 default(旧2块) vs adaptive(自适应) vs quarter(4块) 的性能"""
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh

# 上传新源码
ssh.upload("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")

# 编译三个版本
# adaptive: 新的自适应选择 (默认)
# quarter: 强制 4 块 (DIV_MU_IN_QUARTER)
# old_default: 旧版 2 块 (需要临时改回旧逻辑, 用宏控制)
configs = [
    ("adaptive", "-DDISABLE_2NXN_CYCLIC"),
    ("quarter",  "-DDISABLE_2NXN_CYCLIC -DDIV_MU_IN_QUARTER"),
]

print("=== Compile ===")
for name, flags in configs:
    exe = f"/home/azzr/moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} /home/azzr/moptm_fusion.cpp -o {exe} -pthread 2>&1"
    rc, out, err = ssh.run(cmd, timeout=300)
    print(f"  {name}: {'OK' if rc == 0 else 'FAIL: '+out[:100]}")

# 先验证正确性
print("\n=== Correctness Check ===")
cmd = """python3 -c "
import random, subprocess, sys
sys.set_int_max_str_digits(2000000)
random.seed(2026)
fails = 0
for i in range(20):
    a_d = random.choice([5000, 10000, 50000, 100000, 200000, 500000])
    b_d = a_d // 2
    a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
    b = str(random.randint(1,9))+''.join([str(random.randint(0,9)) for _ in range(b_d-1)])
    A = int(a); B = int(b)
    Q = str(A // B); R = str(A % B)
    inp = f'1\\n{a}\\n{b}\\n'
    with open('/tmp/fuzz.txt','w') as f: f.write(inp)
    for exe in ['moptm_adaptive', 'moptm_quarter']:
        r = subprocess.run([f'/home/azzr/{exe}'], stdin=open('/tmp/fuzz.txt'), capture_output=True, text=True, timeout=60)
        out = r.stdout.strip()
        parts = out.split(' ')
        if len(parts) != 2 or parts[0] != Q or parts[1] != R:
            print(f'  FAIL {exe} case {i}: a={a_d} b={b_d}')
            fails += 1
print(f'  {20*2-fails}/{20*2} PASS')
" """
rc, out, err = ssh.run(cmd, timeout=600)
print(out.strip())

# benchmark
test_cases = [
    ("50k/25k",   50000,  25000),
    ("100k/50k", 100000,  50000),
    ("200k/100k",200000, 100000),
    ("500k/250k",500000, 250000),
    ("1M/500k",  1000000,500000),
]

print(f"\n{'Config':<12} {'Test Case':<12} {'Best (ms)':<12} {'Speedup':<10}")
print("-" * 50)

for tc_name, a_d, b_d in test_cases:
    # 生成测试用例
    cmd = f"""python3 -c "
import random
random.seed(42)
a = ''.join([str(random.randint(0,9)) for _ in range({a_d})])
b = str(random.randint(1,9))+''.join([str(random.randint(0,9)) for _ in range({b_d}-1)])
with open('/tmp/bench.txt','w') as f:
    f.write('1\\n'+a+'\\n'+b+'\\n')
" """
    ssh.run(cmd, timeout=30)
    
    # 预热
    ssh.run("/home/azzr/moptm_adaptive < /tmp/bench.txt > /dev/null 2>&1", timeout=120)
    
    baseline_time = None
    for name, flags in configs:
        exe = f"/home/azzr/moptm_{name}"
        cmd = f"""python3 -c "
import subprocess, time
times = []
for i in range(5):
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
            if name == "adaptive":
                baseline_time = t
            elif baseline_time:
                speedup = f"{baseline_time/t:.2f}x"
            print(f"{name:<12} {tc_name:<12} {t:<12.3f} {speedup:<10}")
        else:
            print(f"{name:<12} {tc_name:<12} {'FAIL':<12} {'-':<10}")
    print()

print("Done!")
