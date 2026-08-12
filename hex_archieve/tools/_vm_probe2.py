import paramiko

HOST="192.168.1.55"; USER="azzr"; PASS = "REDACTED"
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(HOST, username=USER, password=PASS, timeout=30)

def run(cmd):
    stdin, stdout, stderr = ssh.exec_command(cmd)
    out = stdout.read().decode(errors="replace")
    err = stderr.read().decode(errors="replace")
    return out, err

# 1) task.md
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/task.md")
print("=== task.md ===")
print(o)

# 2) checker.cpp
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/checker.cpp")
print("=== checker.cpp ===")
print(o)

# 3) verifier.cpp
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/verifier.cpp")
print("=== verifier.cpp ===")
print(o)

# 4) params.h
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/params.h")
print("=== params.h ===")
print(o)

# 5) info.toml
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/info.toml")
print("=== info.toml ===")
print(o)

# 6) example_00.in 全文（看清 A/B 两数的真正格式）
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/in/example_00.in")
print("=== example_00.in ===")
print(o[:3000])

# 7) example_00.out 官方期望输出（看格式：Q R 怎么排）
o,e = run("cat /home/azzr/lcp/big_integer/division_of_big_integers/out/example_00.out 2>/dev/null || ls /home/azzr/lcp/big_integer/division_of_big_integers/out/ 2>/dev/null | head")
print("=== example_00.out (官方) ===")
print(o[:2000])

ssh.close()
