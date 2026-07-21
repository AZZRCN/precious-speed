#!/usr/bin/env python3
# 验证修复后的正确性: 上传源码, 编译, fuzz 测试
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

CMD_COMPILE_DEFAULT = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV moptm_fusion.cpp -o moptm_default -pthread"
CMD_COMPILE_DISABLE = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DDISABLE_GMP_NEWTON moptm_fusion.cpp -o moptm_disable -pthread"

FUZZ_SCRIPT = """
import random, subprocess, hashlib, os
random.seed(2026)
def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n"

tests = []
for _ in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

fail = 0
for i, (a_d, b_d) in enumerate(tests):
    inp = gen(a_d, b_d)
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r1 = subprocess.run(['bash', '-c', './moptm_default < /tmp/fuzz.in'], capture_output=True, timeout=60)
    r2 = subprocess.run(['bash', '-c', './moptm_disable < /tmp/fuzz.in'], capture_output=True, timeout=60)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    if h1 != h2:
        print(f"FAIL #{i}: a={a_d} b={b_d} default rc={r1.returncode} len={len(r1.stdout)} disable rc={r2.returncode} len={len(r2.stdout)}")
        if r1.stderr:
            print(f"  default stderr: {r1.stderr.decode()[:300]}")
        fail += 1
    elif i % 40 == 0:
        print(f"OK #{i}: a={a_d} b={b_d}")
print(f"=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ===")
os.unlink('/tmp/fuzz.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

# 上传
print("\n=== Upload ===")
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK]")

# 编译
print("\n=== Compile default ===")
stdin, stdout, stderr = c.exec_command(CMD_COMPILE_DEFAULT, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err}")
if rc != 0: exit(1)

print("\n=== Compile disable ===")
stdin, stdout, stderr = c.exec_command(CMD_COMPILE_DISABLE, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err}")
if rc != 0: exit(1)

# Fuzz 200 组
print("\n=== Fuzz test (200 cases, file redirection) ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3", timeout=3600)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

c.close()
