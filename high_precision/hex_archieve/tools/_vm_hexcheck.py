import paramiko, os

HOST="192.168.1.55"; USER="azzr"; PASS = "REDACTED"
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(HOST, username=USER, password=PASS, timeout=30)
sftp = ssh.open_sftp()

LOCAL_FILES = {
    "D:/hex_precious_speed/work/div/v10.cpp": "/home/azzr/divbench/v10.cpp",
    "D:/hex_precious_speed/work/div/v9.cpp":  "/home/azzr/divbench/v9.cpp",
    "D:/hex_precious_speed/submit_ready/div.cpp": "/home/azzr/divbench/v8.cpp",
    "D:/hex_precious_speed/tools/hexcheck.py": "/home/azzr/divbench/hexcheck.py",
    "D:/hex_precious_speed/web_best/div.cpp": "/home/azzr/divbench/web_div.cpp",
}
for local, remote in LOCAL_FILES.items():
    sftp.put(local, remote)
    print("put", os.path.basename(local), "->", remote)

def run(cmd, timeout=600):
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode(errors="replace")
    err = stderr.read().decode(errors="replace")
    return out, err

# 1) 生成 HEX 测试用例 (edges + 随机, 覆盖至 200k hex 位 + 极端形状)
print("\n=== GEN HEX CASES ===")
o,e = run("cd /home/azzr/divbench && python3 hexcheck.py gen div 7 hx.in hx.exp")
print(o); print("ERR:", e[:400])

# 2) 编译四个二进制
print("=== BUILD ===")
for tag, src in [("div_v8","v8.cpp"),("div_v9","v9.cpp"),("div_v10","v10.cpp")]:
    o,e = run(f"cd /home/azzr/divbench && g++ -O2 -std=c++23 -march=x86-64-v3 -o {tag} {src} 2>&1 | tail -5; echo RC=$?")
    print(tag, "->", o.strip()[-200:])
# web_best 参考 (用 GMP, 可能失败, 仅作交叉验证)
o,e = run("cd /home/azzr/divbench && g++ -O2 -std=c++23 -march=x86-64-v3 -o web_div web_div.cpp -ldl 2>&1 | tail -8; echo RC=$?")
print("web_div ->", o.strip()[-300:])

# 3) 用 hexcheck (Python 精确 oracle) 逐一判定
print("\n=== HEXCHECK RUN (Python int oracle) ===")
for tag in ["div_v8","div_v9","div_v10","web_div"]:
    o,e = run(f"cd /home/azzr/divbench && python3 hexcheck.py run ./{tag} hx.in hx.exp 2>&1", timeout=1200)
    print(f"--- {tag} ---")
    print(o.strip()[:1200])

# 4) 交叉验证: web_div vs hexcheck oracle (确认 oracle 自身正确)
print("\n=== cross-check web_div vs oracle ===")
# 上面 web_div 的 hexcheck run 已含与 oracle 比对, 结果见上.

sftp.close(); ssh.close()
print("\nDONE")
