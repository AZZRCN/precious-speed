#!/usr/bin/env python3
# _vm_v10.py — 把 work/div/v10.cpp 推到 192.168.1.55, 编译, 并探查 verify_official 接口。
# 用系统 Python (含 paramiko)。直接 paramiko 硬编码主机, 规避 vmctl .vmhost 缓存坑。
import paramiko, sys, re

HOSTS = ["192.168.1.55", "10.144.33.157", "192.168.1.66", "192.168.1.65", "192.168.1.64"]
USER, PASS = "azzr", "REDACTED"

def connect():
    last = None
    for h in HOSTS:
        try:
            c = paramiko.SSHClient()
            c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            c.connect(h, username=USER, password=PASS, timeout=20)
            print(f"[conn] {h}")
            return c, h
        except Exception as e:
            last = e
            print(f"[fail] {h}: {e}")
    raise last

c, host = connect()
sftp = c.open_sftp()
local = "work/div/v10.cpp"
remote = "/home/azzr/divbench/v10.cpp"
sftp.put(local, remote)
print(f"[put] {local} -> {remote} @ {host}")
sftp.close()

# 编译 (统一口径: g++ -O2 -march=x86-64-v3 -std=c++23)
cmd = ("cd /home/azzr/divbench && "
       "g++ -O2 -march=x86-64-v3 -std=c++23 v10.cpp -o div_v10 2>&1 && echo BUILD_OK")
_, so, se = c.exec_command(cmd)
out = so.read().decode(errors='replace')
err = se.read().decode(errors='replace')
print("=== BUILD ===")
print(out, err)
if "BUILD_OK" not in out:
    print("[abort] build failed")
    c.close(); sys.exit(1)

# 探查 verify_official.py 接口
_, so2, se2 = c.exec_command("echo '--- head ---'; head -50 /home/azzr/verify_official.py; "
                              "echo '--- argv/argparse ---'; "
                              "grep -nE 'sys.argv|argparse|add_argument|def main|hash.json|in/|getopt' /home/azzr/verify_official.py | head -40")
print("=== verify_official interface ===")
print(so2.read().decode(errors='replace'))
print(se2.read().decode(errors='replace'))

c.close()
print("[done] host=", host)
