#!/usr/bin/env python3
# 简单性能测试
import paramiko, sys, random, base64, time

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 上传源码
sftp = c.open_s_sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()

# 编译
configs = [
    ("baseline", "-DDISABLE_2NXN_CYCLIC"),
    ("gmp_nocyclic", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=1000000"),
]

for name, flags in configs:
    cmd = f"cd /home/azzr && g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o moptm_{name} -pthread 2>&1"
    print(f"Compiling {name}...")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  FAILED")
        c.close()
        sys.exit(1)
print("Compiled OK\n")

# 生成一个测试用例（中等大小）
random.seed(42)
a_d = 200000
b_d = 100000
a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
test_input = f"1\n{a}\n{b}\n"
test_b64 = base64.b64encode(test_input.encode()).decode()

cmd = f"echo {test_b64} | base64 -d > /tmp/bench.in"
c.exec_command(cmd)

# 用 /usr/bin/time 运行多次
for name, flags in configs:
    exe = f"moptm_{name}"
    print(f"=== {name} (a={a_d}, b={b_d}) ===")
    times = []
    for i in range(5):
        cmd = f"cd /home/azzr && /usr/bin/time -f '%e' ./{exe} < /tmp/bench.in > /tmp/bench_{name}.out 2>&1"
        stdin, stdout, stderr = c.exec_command(cmd, timeout=120)
        rc = stdout.channel.recv_exit_status()
        # 读取 time 输出
        cmd2 = f"tail -1 /tmp/bench_{name}.out"
        stdin2, stdout2, stderr2 = c.exec_command(cmd2)
        t_str = stdout2.read().decode().strip()
        try:
            t = float(t_str)
            times.append(t)
            print(f"  Run {i+1}: {t:.3f}s")
        except:
            print(f"  Run {i+1}: ERROR ({t_str})")
    if times:
        best = min(times)
        avg = sum(times) / len(times)
        print(f"  Best: {best:.3f}s, Avg: {avg:.3f}s")
    print()

c.close()
