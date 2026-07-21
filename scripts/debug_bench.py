#!/usr/bin/env python3
import paramiko, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 测试 bench.sh
print("\n=== Test bench.sh ===")
cmd = "cd /home/azzr && bash -x bench.sh moptm_baseline 10000 5000 1 2>&1"
stdin, stdout, stderr = c.exec_command(cmd, timeout=60)
print(stdout.read().decode()[:2000])
err = stderr.read().decode()
if err: print(f"[STDERR] {err[:1000]}")

c.close()
