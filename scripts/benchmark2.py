#!/usr/bin/env python3
import paramiko, sys, time, random, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

def generate_test(a_digits, b_digits, seed=42):
    random.seed(seed)
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\n{a}\n{b}\n"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 上传源码
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded")

# 编译配置
configs = [
    ("baseline", "-DDISABLE_2NXN_CYCLIC"),
    ("gmp_fallback", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=1000000"),
]

for name, flags in configs:
    exe = f"moptm_bench_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread"
    print(f"\n=== Compile {name} ===")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  COMPILE FAILED: {stderr.read().decode()[:500]}")
        c.close()
        sys.exit(1)
    print(f"  [OK]")

# 性能测试用例（先测小的，逐步加大）
test_cases = [
    ("10k/5k", 10000, 5000),
    ("50k/25k", 50000, 25000),
    ("100k/50k", 100000, 50000),
]

print(f"\n{'Config':<20} {'Test Case':<12} {'Time (s)':<12} {'Speedup':<10}")
print("-" * 60)

baseline_times = {}

for tc_name, a_d, b_d in test_cases:
    test_input = generate_test(a_d, b_d, seed=123)
    test_b64 = base64.b64encode(test_input.encode()).decode()
    
    for name, flags in configs:
        exe = f"moptm_bench_{name}"
        
        # 用 Python 计时
        cmd = f"echo {test_b64} | base64 -d > /tmp/bench_{name}_{tc_name.replace('/','_')}.in"
        stdin, stdout, stderr = c.exec_command(cmd, timeout=30)
        stdout.channel.recv_exit_status()
        
        # 运行并计时
        start = time.time()
        cmd = f"cd /home/azzr && ./{exe} < /tmp/bench_{name}_{tc_name.replace('/','_')}.in > /tmp/bench_{name}_{tc_name.replace('/','_')}.out"
        stdin, stdout, stderr = c.exec_command(cmd, timeout=120)
        rc = stdout.channel.recv_exit_status()
        elapsed = time.time() - start
        
        if rc != 0:
            print(f"{name:<20} {tc_name:<12} {'FAIL':<12} {'-':<10}")
            continue
        
        speedup = ""
        if name == "baseline":
            baseline_times[tc_name] = elapsed
        else:
            if tc_name in baseline_times and baseline_times[tc_name] > 0:
                sp = baseline_times[tc_name] / elapsed
                speedup = f"{sp:.2f}x"
        print(f"{name:<20} {tc_name:<12} {elapsed:<12.3f} {speedup:<10}")
        sys.stdout.flush()

c.close()
print("\nDone!")
