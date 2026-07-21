#!/usr/bin/env python3
# 用文件重定向 (而非 pipe) 重新 fuzz 测试, 消除 pipe 缓冲区问题
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

FUZZ_SCRIPT = """
import random, subprocess, hashlib, os
random.seed(2026)
def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n"

tests = []
for _ in range(100):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

fail = 0
for i, (a_d, b_d) in enumerate(tests):
    inp = gen(a_d, b_d)
    # 写入临时文件, 用 shell 重定向
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r1 = subprocess.run(['bash', '-c', './moptm_default < /tmp/fuzz.in'], capture_output=True, timeout=60)
    r2 = subprocess.run(['bash', '-c', './moptm_disable < /tmp/fuzz.in'], capture_output=True, timeout=60)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    if h1 != h2:
        print(f"FAIL #{i}: a={a_d} b={b_d} default rc={r1.returncode} len={len(r1.stdout)} disable rc={r2.returncode} len={len(r2.stdout)}")
        print(f"  default md5={h1[:8]} disable md5={h2[:8]}")
        if r1.stderr:
            print(f"  default stderr: {r1.stderr.decode()[:300]}")
        fail += 1
    elif i % 20 == 0:
        print(f"OK #{i}: a={a_d} b={b_d}")
print(f"=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ===")
os.unlink('/tmp/fuzz.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

print("\n=== Fuzz test with file redirection (100 cases) ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3", timeout=1800)
out = stdout.read().decode()
err = stderr.read().decode()
print(out)
if err: print(f"[STDERR] {err}")

c.close()
