#!/usr/bin/env python3
# 捕获 absInvNewtonGMP cyclic 路径的 FAIL case
import paramiko, base64, sys, random

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

# 生成一个特定大小的测试用例，更容易触发 FAIL
# 之前 FAIL 的是 a=500000 b=250000 (CYCLIC_MIN_K=4096)
# in ≈ b_digits / 4 ≈ 62500 limbs，大于 4096，所以走 cyclic

FUZZ_SCRIPT = """
import random, subprocess, os, sys
sys.set_int_max_str_digits(2000000)

seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
random.seed(seed)
target = sys.argv[2] if len(sys.argv) > 2 else "moptm_test"

fail = 0
for i in range(20):
    # 固定大小: a=500000位, b=250000位 (容易触发 FAIL)
    a_d = 500000
    b_d = 250000
    a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
    inp = f"1\\n{a}\\n{b}\\n"
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r = subprocess.run(['bash', '-c', f'./{target} < /tmp/fuzz.in'], capture_output=True, timeout=120)
    A = int(a)
    B = int(b)
    Q = A // B
    R = A % B
    sq, sr = str(Q), str(R)
    try:
        line = r.stdout.decode().strip()
        parts = line.split(' ')
        mq, mr = parts[0], parts[1]
        ok = (mq == sq) and (mr == sr)
    except:
        ok = False
    if not ok:
        fail += 1
        # 输出 FAIL case 的信息
        print(f"FAIL #{i}: seed={seed} a_digits={a_d} b_digits={b_d}")
        # 商差多少位
        q_ok = (mq == sq)
        r_ok = (mr == sr)
        print(f"  q_ok={q_ok} r_ok={r_ok}")
        if not q_ok:
            print(f"  q_len_correct={len(sq)} q_len_mine={len(mq)}")
            # 找第一个不同的位置
            min_len = min(len(sq), len(mq))
            for j in range(min_len):
                if sq[j] != mq[j]:
                    print(f"  first_diff_pos={j} sq[{j}:{j+20}]={sq[j:j+20]} mq[{j}:{j+20}]={mq[j:j+20]}")
                    break
        sys.stdout.flush()
        break  # 找到一个就退出

print(f"\\nResult: {fail} FAIL out of {i+1} tests")
os.unlink('/tmp/fuzz.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded")

# 编译两个版本进行对比
configs = [
    ("gmp_cyc4096", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=4096"),
    ("gmp_bigk", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC -DCYCLIC_MIN_K=1000000"),
]

for name, flags in configs:
    exe = f"moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread"
    print(f"\n=== Compile {name} ===")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  COMPILE FAILED: {stderr.read().decode()[:500]}")
        c.close()
        sys.exit(1)
    print(f"  [OK]")

# 用多个 seed 测试，找到 FAIL
print(f"\n=== Searching for FAIL case... ===")
for seed in range(0, 100):
    print(f"Seed {seed}...", end=" ", flush=True)
    stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3 - {seed} moptm_gmp_cyc4096", timeout=600)
    out = stdout.read().decode()
    if "FAIL" in out:
        print("FAIL!")
        print(out)
        break
    else:
        print("PASS")

c.close()
