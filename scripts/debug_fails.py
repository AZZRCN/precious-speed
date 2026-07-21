#!/usr/bin/env python3
# 用 gdb 调试 FAIL #65 (SIGABRT) 和复现 FAIL #71
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

DEBUG_SCRIPT = """
import random, subprocess, hashlib
random.seed(2026)
def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n"

tests = []
for _ in range(80):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 生成 FAIL #65 和 #71 的输入
for i in [65, 71]:
    a_d, b_d = tests[i]
    # 重新生成随机数到第 i 个
    random.seed(2026)
    for j in range(i+1):
        a_d2, b_d2 = tests[j]
        a = ''.join([str(random.randint(0,9)) for _ in range(a_d2)])
        b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d2-1)])
    inp = f"1\\n{a}\\n{b}\\n"
    fname = f"/tmp/fail{i}.in"
    with open(fname, 'w') as f:
        f.write(inp)
    print(f"FAIL #{i}: a={a_d} b={b_d} -> {fname}")
    print(f"  a_limbs={(a_d+3)//4}, b_limbs={(b_d+3)//4}")

# 验证两个版本
for i in [65, 71]:
    fname = f"/tmp/fail{i}.in"
    r1 = subprocess.run(['./moptm_default'], input=open(fname,'rb').read(), capture_output=True, timeout=30)
    r2 = subprocess.run(['./moptm_disable'], input=open(fname,'rb').read(), capture_output=True, timeout=30)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    print(f"FAIL #{i}: default rc={r1.returncode} len={len(r1.stdout)} md5={h1[:8]}")
    print(f"         disable rc={r2.returncode} len={len(r2.stdout)} md5={h2[:8]}")
    if r1.stderr:
        print(f"         default stderr: {r1.stderr.decode()[:300]}")
"""
DEBUG_B64 = base64.b64encode(DEBUG_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

# 1. 生成输入文件并验证
print("\n=== Step 1: Generate & verify FAIL inputs ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {DEBUG_B64} | base64 -d | python3", timeout=120)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

# 2. 用 gdb 运行 FAIL #65
print("\n=== Step 2: gdb FAIL #65 ===")
stdin, stdout, stderr = c.exec_command(
    "cd /home/azzr && gdb -batch -ex 'run < /tmp/fail65.in' -ex 'bt' -ex 'info registers' ./moptm_default 2>&1 | tail -50",
    timeout=60
)
print(stdout.read().decode())

# 3. 用 gdb 运行 FAIL #71
print("\n=== Step 3: gdb FAIL #71 ===")
stdin, stdout, stderr = c.exec_command(
    "cd /home/azzr && gdb -batch -ex 'run < /tmp/fail71.in' -ex 'bt' -ex 'info registers' ./moptm_default 2>&1 | tail -50",
    timeout=60
)
print(stdout.read().decode())

c.close()
