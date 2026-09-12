#!/usr/bin/env python3
"""三合一大规模验证启动器: 上传 moptm_fusion.cpp + vm_verify_all.py 到 VM 执行"""
import paramiko, sys

HOST = "10.144.33.157"
USER = "azzr"
PWD = "REDACTED"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.put("d:/precious_speed/scripts/vm_verify_all.py", "/home/azzr/vm_verify_all.py")
sftp.close()
print("[OK] Uploaded moptm_fusion.cpp + vm_verify_all.py")

print("\n=== Running verification (ADD+MUL+DIV, ~2130 cases) ===")
stdin, stdout, stderr = c.exec_command(
    "cd /home/azzr && python3 vm_verify_all.py",
    timeout=1800
)
out = stdout.read().decode()
print(out)
err = stderr.read().decode()
if err:
    print("[STDERR] " + err[:500])

c.close()
print("[DONE]")
