#!/usr/bin/env python3
import paramiko, sys, random, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 生成测试用例，用 base64 编码传输
random.seed(42)
a_d = 50000
b_d = 25000
a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
test_input = f"1\n{a}\n{b}\n"
test_b64 = base64.b64encode(test_input.encode()).decode()

cmd = f"echo {test_b64} | base64 -d > /tmp/test_big.in"
stdin, stdout, stderr = c.exec_command(cmd, timeout=30)
print(f"Write test input: exit {stdout.channel.recv_exit_status()}")

# 运行
print("\n=== Running debug version ===")
cmd = "cd /home/azzr && timeout 60 ./moptm_debug < /tmp/test_big.in > /tmp/test_out.txt 2>&1"
stdin, stdout, stderr = c.exec_command(cmd, timeout=120)
rc = stdout.channel.recv_exit_status()
print(f"Exit code: {rc}")

# 查看调试输出
cmd = "grep '\\[DEBUG-CYC\\]' /tmp/test_out.txt | head -10"
stdin, stdout, stderr = c.exec_command(cmd)
out = stdout.read().decode()
print("\n=== Debug output (first 10) ===")
print(out)

if not out:
    print("No debug output. Checking output file size and first lines:")
    cmd = "wc -c /tmp/test_out.txt && head -5 /tmp/test_out.txt"
    stdin, stdout, stderr = c.exec_command(cmd)
    print(stdout.read().decode())

c.close()
