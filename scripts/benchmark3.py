#!/usr/bin/env python3
# 在 VM 内部运行 benchmark，获得准确的性能数据
import paramiko, sys, random, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 上传源码
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded")

# 在 VM 上写一个 benchmark 脚本
bench_script = '''
#!/bin/bash
# 用法: bench.sh <exe> <a_digits> <b_digits> <iterations>

exe=$1
a_d=$2
b_d=$3
iters=$4

# 生成测试用例
python3 -c "
import random, sys
random.seed(123)
a = ''.join([str(random.randint(0,9)) for _ in range($a_d)])
b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range($b_d-1)])
print(f'1\\n{a}\\n{b}')
" > /tmp/bench_input.txt

# 预热
./$exe < /tmp/bench_input.txt > /dev/null 2>&1

# 运行 iters 次，取最好时间
best=999999
for i in $(seq 1 $iters); do
    t=$(time -p ./$exe < /tmp/bench_input.txt 2>&1 >/dev/null | grep real | awk '{print $2}')
    if python3 -c "exit(0 if $t < $best else 1)"; then
        best=$t
    fi
done
echo $best
'''

# 写入脚本
cmd = f"cat > /home/azzr/bench.sh << 'EOF'\n{bench_script}\nEOF\nchmod +x /home/azzr/bench.sh"
stdin, stdout, stderr = c.exec_command(cmd)
print(f"Write bench.sh: exit {stdout.channel.recv_exit_status()}")

# 编译配置
configs = [
    ("baseline", "-DDISABLE_2NXN_CYCLIC"),
    ("gmp_fallback", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=1000000"),
    ("gmp_cyc16k", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=16384"),
    ("gmp_cyc8k", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=8192"),
    ("gmp_cyc4k", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=4096"),
]

for name, flags in configs:
    exe = f"moptm_{name}"
    cmd = f"cd /home/azzr && g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread 2>&1"
    print(f"\n=== Compile {name} ===")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  FAILED: {stdout.read().decode()[:300]}")
    else:
        print(f"  OK")

# 测试用例
test_cases = [
    ("50k/25k", 50000, 25000),
    ("100k/50k", 100000, 50000),
    ("200k/100k", 200000, 100000),
    ("500k/250k", 500000, 250000),
]

iters = 3

print(f"\n{'Config':<20} {'Test Case':<12} {'Best Time (s)':<15} {'Speedup':<10}")
print("-" * 60)

baseline_times = {}

for tc_name, a_d, b_d in test_cases:
    for name, flags in configs:
        exe = f"moptm_{name}"
        cmd = f"cd /home/azzr && bash bench.sh {exe} {a_d} {b_d} {iters} 2>&1"
        try:
            stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
            rc = stdout.channel.recv_exit_status()
            out = stdout.read().decode().strip()
            if rc == 0 and out:
                t = float(out)
                speedup = ""
                if name == "baseline":
                    baseline_times[tc_name] = t
                elif tc_name in baseline_times and baseline_times[tc_name] > 0:
                    sp = baseline_times[tc_name] / t
                    speedup = f"{sp:.2f}x"
                print(f"{name:<20} {tc_name:<12} {t:<15.3f} {speedup:<10}")
            else:
                print(f"{name:<20} {tc_name:<12} {'FAIL':<15} {'-':<10}")
        except Exception as e:
            print(f"{name:<20} {tc_name:<12} {'ERROR':<15} {'-':<10}")
        sys.stdout.flush()

c.close()
print("\nDone!")
