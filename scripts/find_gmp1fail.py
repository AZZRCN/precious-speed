#!/usr/bin/env python3
# 找到并分析 absInvNewtonGMP cyclic 路径的 FAIL case
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

# 编译两个版本
configs = [
    ("gmp_cyc4k", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=4096"),
    ("gmp_bigk", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=1000000"),
]

for name, flags in configs:
    cmd = f"cd /home/azzr && g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o moptm_{name} -pthread 2>&1"
    print(f"Compiling {name}...")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  FAILED: {stdout.read().decode()[:300]}")
        c.close()
        sys.exit(1)
print("All compiled")

# 搜索 FAIL case：用之前发现的 FAIL 大小 a=500000 b=250000
print("\n=== Searching for FAIL case (500k/250k) ===")
found = False
for seed in range(0, 200):
    random.seed(seed)
    a_d = 500000
    b_d = 250000
    a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
    test_input = f"1\n{a}\n{b}\n"
    test_b64 = base64.b64encode(test_input.encode()).decode()
    
    cmd = f"echo {test_b64} | base64 -d > /tmp/fuzz.in && cd /home/azzr && ./moptm_gmp_cyc4k < /tmp/fuzz.in > /tmp/fuzz.out 2>&1"
    stdin, stdout, stderr = c.exec_command(cmd, timeout=120)
    rc = stdout.channel.recv_exit_status()
    
    if rc != 0:
        print(f"Seed {seed}: CRASH (exit {rc})")
        continue
    
    # 用 Python 计算正确答案
    A = int(a)
    B = int(b)
    Q = A // B
    R = A % B
    
    # 读取输出
    cmd = "head -1 /tmp/fuzz.out"
    stdin, stdout, stderr = c.exec_command(cmd)
    out = stdout.read().decode().strip()
    try:
        parts = out.split(' ')
        mq, mr = parts[0], parts[1]
        q_ok = (mq == str(Q))
        r_ok = (mr == str(R))
        if not q_ok or not r_ok:
            print(f"Seed {seed}: FAIL (q_ok={q_ok}, r_ok={r_ok})")
            found = True
            # 保存这个 case
            with open(f"d:/precious_speed/fail_case_seed{seed}.txt", "w") as f:
                f.write(f"seed={seed}\n")
                f.write(f"a_digits={a_d}\n")
                f.write(f"b_digits={b_d}\n")
                f.write(f"a={a}\n")
                f.write(f"b={b}\n")
                f.write(f"correct_q={Q}\n")
                f.write(f"correct_r={R}\n")
                f.write(f"gmp_cyc4k_q={mq}\n")
                f.write(f"gmp_cyc4k_r={mr}\n")
            print(f"  Saved to fail_case_seed{seed}.txt")
            break
    except Exception as e:
        print(f"Seed {seed}: PARSE ERROR ({e})")

if not found:
    print("No FAIL found in first 200 seeds")

c.close()
