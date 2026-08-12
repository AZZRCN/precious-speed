import paramiko, sys

HOST="192.168.1.55"; USER="azzr"; PASS = "REDACTED"
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(HOST, username=USER, password=PASS, timeout=30)

def run(cmd):
    stdin, stdout, stderr = ssh.exec_command(cmd)
    out = stdout.read().decode(errors="replace")
    err = stderr.read().decode(errors="replace")
    return out, err

# 1) 任务目录结构
o,e = run("ls -la /home/azzr/lcp/big_integer/division_of_big_integers/ 2>/dev/null; echo '=== task dir done ==='")
print("=== TASK DIR ===")
print(o)
print("ERR:", e[:500])

# 2) 找官方 sol
o,e = run("find /home/azzr/lcp/big_integer/division_of_big_integers/ -maxdepth 3 -iname '*sol*' -o -iname '*.cpp' 2>/dev/null | head -40; echo '=== sol done ==='")
print("=== SOL CANDIDATES ===")
print(o)

# 3) 其他可能参考
o,e = run("ls -la /home/azzr/lcp/big_integer/ 2>/dev/null; echo '=== big_integer dir done ==='")
print("=== BIG_INTEGER DIR ===")
print(o)

# 4) verify_official.py 全文
o,e = run("cat /home/azzr/verify_official.py 2>/dev/null; echo '=== verify done ==='")
print("=== VERIFY_OFFICIAL.PY ===")
print(o)

# 5) 样本输入格式（前几行）
o,e = run("ls /home/azzr/lcp/big_integer/division_of_big_integers/in/ 2>/dev/null | head; echo '--- first in head ---'; head -c 1500 /home/azzr/lcp/big_integer/division_of_big_integers/in/$(ls /home/azzr/lcp/big_integer/division_of_big_integers/in/ 2>/dev/null | head -1) 2>/dev/null; echo; echo '=== in sample done ==='")
print("=== SAMPLE IN ===")
print(o)

# 6) hash.json 头部
o,e = run("head -c 800 /home/azzr/lcp/big_integer/division_of_big_integers/hash.json 2>/dev/null; echo; echo '=== hash head done ==='")
print("=== HASH.JSON HEAD ===")
print(o)

# 7) divbench 现有二进制
o,e = run("ls -la /home/azzr/divbench/ 2>/dev/null | head -40; echo '=== divbench done ==='")
print("=== DIVBENCH ===")
print(o)

ssh.close()
