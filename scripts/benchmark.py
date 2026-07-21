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
    ("gmp_cyc4k", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=4096"),
    # 暂时不测 absDivMu cyclic，因为有 bug
]

for name, flags in configs:
    exe = f"moptm_bench_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread"
    print(f"\n=== Compile {name} ===")
    print(f"  {flags}")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  COMPILE FAILED: {stderr.read().decode()[:500]}")
        continue
    print(f"  [OK]")

# 性能测试用例
test_cases = [
    ("10k/5k", 10000, 5000),
    ("50k/25k", 50000, 25000),
    ("100k/50k", 100000, 50000),
    ("200k/100k", 200000, 100000),
    ("500k/250k", 500000, 250000),
    ("1M/500k", 1000000, 500000),
]

print("\n" + "=" * 70)
print(f"{'Config':<20} {'Test Case':<12} {'Time (s)':<12} {'Speedup':<10}")
print("=" * 70)

baseline_times = {}

for name, flags in configs:
    exe = f"moptm_bench_{name}"
    for tc_name, a_d, b_d in test_cases:
        test_input = generate_test(a_d, b_d, seed=123)
        test_b64 = base64.b64encode(test_input.encode()).decode()
        
        # 运行 3 次取最好
        best_time = None
        for run in range(3):
            cmd = f"echo {test_b64} | base64 -d > /tmp/bench.in && cd /home/azzr && time -p timeout 120 ./{exe} < /tmp/bench.in > /tmp/bench.out 2>&1"
            stdin, stdout, stderr = c.exec_command(cmd, timeout=180)
            rc = stdout.channel.recv_exit_status()
            
            if rc != 0:
                best_time = None
                break
            
            # 从 time -p 输出中读取 real time
            cmd2 = "grep 'real' /tmp/bench.out"
            stdin2, stdout2, stderr2 = c.exec_command(cmd2)
            time_out = stdout2.read().decode().strip()
            try:
                t = float(time_out.split()[1])
                if best_time is None or t < best_time:
                    best_time = t
            except:
                pass
        
        if best_time is None:
            print(f"{name:<20} {tc_name:<12} {'CRASH/FAIL':<12} {'-':<10}")
        else:
            speedup = ""
            if name == "baseline":
                baseline_times[tc_name] = best_time
            else:
                if tc_name in baseline_times and baseline_times[tc_name] > 0:
                    sp = baseline_times[tc_name] / best_time
                    speedup = f"{sp:.2f}x"
            print(f"{name:<20} {tc_name:<12} {best_time:<12.3f} {speedup:<10}")
        sys.stdout.flush()

c.close()
print("\nDone!")
