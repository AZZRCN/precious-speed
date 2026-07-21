#!/usr/bin/env python3
# 重新生成 FAIL 输入并保存到文件, 用文件测试
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

# 只运行 Step 1, 保存到文件查看完整输出
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
    random.seed(2026)
    for j in range(i+1):
        a_d2, b_d2 = tests[j]
        a = ''.join([str(random.randint(0,9)) for _ in range(a_d2)])
        b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d2-1)])
    inp = f"1\\n{a}\\n{b}\\n"
    fname = f"/tmp/fail{i}.in"
    with open(fname, 'w') as f:
        f.write(inp)
    print(f"FAIL #{i}: a={a_d} b={b_d} -> {fname} (a_limbs={(a_d+3)//4}, b_limbs={(b_d+3)//4})", flush=True)

# 验证 (单独运行)
for i in [65, 71]:
    fname = f"/tmp/fail{i}.in"
    r1 = subprocess.run(['./moptm_default'], input=open(fname,'rb').read(), capture_output=True, timeout=60)
    r2 = subprocess.run(['./moptm_disable'], input=open(fname,'rb').read(), capture_output=True, timeout=60)
    h1 = hashlib.md5(r1.stdout).hexdigest()
    h2 = hashlib.md5(r2.stdout).hexdigest()
    print(f"FAIL #{i} single run: default rc={r1.returncode} len={len(r1.stdout)} md5={h1[:8]}", flush=True)
    print(f"                  disable rc={r2.returncode} len={len(r2.stdout)} md5={h2[:8]}", flush=True)
    if r1.returncode != 0:
        print(f"                  default stderr: {r1.stderr.decode()[:500]}", flush=True)
    if h1 != h2:
        print(f"                  MISMATCH! diff in first 200 bytes:", flush=True)
        for k in range(min(200, len(r1.stdout), len(r2.stdout))):
            if r1.stdout[k] != r2.stdout[k]:
                print(f"                  first diff at byte {k}: default={r1.stdout[k:k+20]} disable={r2.stdout[k:k+20]}", flush=True)
                break
"""
DEBUG_B64 = base64.b64encode(DEBUG_SCRIPT.encode()).decode()

stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {DEBUG_B64} | base64 -d | python3", timeout=300)
out = stdout.read().decode()
err = stderr.read().decode()
print("=== STDOUT ===")
print(out)
if err:
    print("=== STDERR ===")
    print(err)

c.close()
