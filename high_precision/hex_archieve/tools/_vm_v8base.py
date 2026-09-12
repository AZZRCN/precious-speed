#!/usr/bin/env python3
# _vm_v8base.py — 在 192.168.1.55 上编译并提交 submit_ready/div.cpp (v8) 跑黄金验证,
# 确认它是否真的是 AC 基线。这是诊断 v10 失败的"地基"前提。
import paramiko, sys

HOSTS = ["192.168.1.55", "10.144.33.157", "192.168.1.66", "192.168.1.65", "192.168.1.64"]
USER, PASS = "azzr", "REDACTED"

def connect():
    last = None
    for h in HOSTS:
        try:
            c = paramiko.SSHClient()
            c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            c.connect(h, username=USER, password=PASS, timeout=20)
            return c, h
        except Exception as e:
            last = e
    raise last

c, host = connect()
sftp = c.open_sftp()
sftp.put("submit_ready/div.cpp", "/home/azzr/divbench/div_v8.cpp")
sftp.close()
print(f"[put] submit_ready/div.cpp -> div_v8.cpp @ {host}")

cmd = ("cd /home/azzr/divbench && "
       "g++ -O2 -march=x86-64-v3 -std=c++23 div_v8.cpp -o div_v8 2>&1 && echo BUILD_OK && "
       "python3 /home/azzr/verify_official.py div ./div_v8 --timeout 120 2>&1")
_, so, se = c.exec_command(cmd)
print(so.read().decode(errors='replace'))
print(se.read().decode(errors='replace'), end='')
c.close()
print("[done]")
