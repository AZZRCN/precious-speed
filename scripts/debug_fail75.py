#!/usr/bin/env python3
# 调查 FAIL #75 (a=200000, b=66664) 的 bug
import paramiko
import base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

DEBUG_SCRIPT = """
import random, subprocess
random.seed(2026)
# 重现 FAIL #75
tests = []
for _ in range(100):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 生成 FAIL #75 的输入
a_d, b_d = tests[75]
print(f"Reproducing FAIL #75: a={a_d} b={b_d}")
random.seed(2026)  # 重置种子
# 需要重新生成到 #75
for i in range(76):
    a_d2, b_d2 = tests[i]
    a = ''.join([str(random.randint(0,9)) for _ in range(a_d2)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d2-1)])

# 生成 #75 的输入
a_d, b_d = tests[75]
a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
inp = f"1\\n{a}\\n{b}\\n"

# 保存输入到文件
with open('/tmp/fail75.in', 'w') as f:
    f.write(inp)
print(f"Input saved to /tmp/fail75.in (a={a_d} digits, b={b_d} digits)")
print(f"b limbs = {(b_d+3)//4}, a limbs = {(a_d+3)//4}")

# 运行 default 版本
print("\\n=== Running moptm_default ===")
r = subprocess.run(['./moptm_default'], input=inp.encode(), capture_output=True, timeout=30)
print(f"returncode: {r.returncode}")
print(f"stdout len: {len(r.stdout)}")
print(f"stderr: {r.stderr.decode()[:500]}")
if r.stdout:
    print(f"stdout first 100: {r.stdout[:100].decode()}")
"""
DEBUG_B64 = base64.b64encode(DEBUG_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected to {USER}@{HOST}")

print("\n=== Debug FAIL #75 ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {DEBUG_B64} | base64 -d | python3", timeout=120)
out = stdout.read().decode()
err = stderr.read().decode()
rc = stdout.channel.recv_exit_status()
print(f"[EXIT {rc}]")
print(out)
if err: print(f"[STDERR] {err}")

# 也测试 50k/25k 的异常
print("\n=== Test 50k/25k with default ===")
stdin, stdout, stderr = c.exec_command("cd /home/azzr && ./moptm_default < div_50k_25k.in | wc -c", timeout=30)
print(f"50k/25k default output bytes: {stdout.read().decode().strip()}")

c.close()
