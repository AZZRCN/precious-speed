#!/usr/bin/env python3
"""隔离 #164 bug: cyclic Newton 逆元 vs 2NXN cyclic 乘积"""
import paramiko, base64

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# 4 个版本: (名字, 额外编译选项)
VERSIONS = [
    ("both",      "-DUSE_GMP_NEWTON"),                    # 两个 cyclic 都启用
    ("newton_only", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC"),  # 只 cyclic Newton
    ("cyclic2n_only", "-DDISABLE_GMP_NEWTON"),             # 只 2NXN cyclic (Newton 走 linear)
    ("baseline",  "-DDISABLE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC"),  # 都禁用
]

# #164 的具体输入 (seed=2026, a=500000, b=250000)
# 需要复现完全相同的输入
GEN_SCRIPT = """
import random
random.seed(2026)

def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return a, b

# 先构建 tests 列表 (与 fuzz 脚本完全一致)
tests = []
for _ in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 生成前 164 组的数据 (消耗随机数), 然后生成 #164
for i in range(165):
    a_d, b_d = tests[i]
    a, b = gen(a_d, b_d)
    if i == 164:
        with open('/tmp/test164.in', 'w') as f:
            f.write(f"1\\n{a}\\n{b}\\n")
        print(f"#164: a_d={a_d} b_d={b_d} a_len={len(a)} b_len={len(b)}")
"""

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected")

# 上传源码
print("\n=== Upload ===")
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK]")

# 生成 #164 测试数据
print("\n=== Generate #164 test data ===")
GEN_B64 = base64.b64encode(GEN_SCRIPT.encode()).decode()
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {GEN_B64} | base64 -d | python3", timeout=60)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

# 编译并测试每个版本
results = {}
for name, flags in VERSIONS:
    print(f"\n=== Compile {name} ({flags}) ===")
    cmd = f"cd /home/azzr && g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o moptm_{name} -pthread 2>&1"
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    err = stderr.read().decode()
    out = stdout.read().decode()
    if out: print(out)
    if err: print(f"[STDERR] {err}")
    if rc != 0:
        print(f"[FAIL] Compile {name} rc={rc}")
        continue

    # 运行 #164
    cmd = f"cd /home/azzr && ./moptm_{name} < /tmp/test164.in"
    stdin, stdout, stderr = c.exec_command(cmd, timeout=60)
    out = stdout.read().decode()
    rc = stdout.channel.recv_exit_status()
    results[name] = out.strip()
    print(f"  Output: {out.strip()[:100]} (rc={rc})")

# 对比结果
print("\n=== Comparison ===")
baseline = results.get("baseline", "")
for name, out in results.items():
    match = "MATCH" if out == baseline else "DIFF"
    print(f"  {name:20s}: {match}  output={out[:60]}")

c.close()
