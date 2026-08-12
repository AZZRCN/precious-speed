#!/usr/bin/env python3
# _vm_v10verify.py — 在 192.168.1.55 上对 div_v10 跑 verify_official (黄金基准)。
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
cmd = ("cd /home/azzr/divbench && "
       "python3 /home/azzr/verify_official.py div ./div_v10 --timeout 120 2>&1")
print(f"[run] {cmd} @ {host}")
_, so, se = c.exec_command(cmd)
print(so.read().decode(errors='replace'))
print(se.read().decode(errors='replace'), end='')
c.close()
print("[done]")
