#!/usr/bin/env python3
# 调试: 在 cyclic 路径中同时计算 full product 和 cyclic product，比较差异
import paramiko, base64, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

FUZZ_SCRIPT = """
import random, subprocess, os, sys
sys.set_int_max_str_digits(2000000)
random.seed(2026)

def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n", a, b

tests = []
for i in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

import sys
target = sys.argv[1] if len(sys.argv) > 1 else "moptm_test"

fail = 0
for i, (a_d, b_d) in enumerate(tests):
    inp, sa, sb = gen(a_d, b_d)
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r = subprocess.run(['bash', '-c', f'./{target} < /tmp/fuzz.in'], capture_output=True, timeout=120)
    A = int(sa)
    B = int(sb)
    Q = A // B
    R = A % B
    sq, sr = str(Q), str(R)
    try:
        line = r.stdout.decode().strip()
        parts = line.split(' ')
        mq, mr = parts[0], parts[1]
        ok = (mq == sq) and (mr == sr)
    except:
        ok = False
    if not ok:
        fail += 1
        # 只打印前 3 个 FAIL 的详情
        if fail <= 3:
            print(f"FAIL #{i}: a={a_d} b={b_d}")
            print(f"  q_len: correct={len(sq)} got={len(mq)}")
            print(f"  q_head correct: {sq[:30]}")
            print(f"  q_head got:     {mq[:30]}")
            sys.stdout.flush()

print(f"\\n=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ({target})")
os.unlink('/tmp/fuzz.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded")

# 测试: 去掉 cx-cy 修正，看看效果
# 我们先测试一下：只做 unwrap，不做 cx-cy 修正
# 用 sed 临时修改源码太麻烦，让我直接在 Python 里生成一个调试版本
# 实际上，让我直接测试多个配置

configs = [
    ("baseline", "-DDISABLE_2NXN_CYCLIC"),
    ("cyclic_orig", ""),
]

for name, flags in configs:
    exe = f"moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread"
    print(f"\n=== Compile {name} ===")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    if rc != 0:
        print(f"  COMPILE FAILED: {stderr.read().decode()[:500]}")
        continue
    print(f"  [OK]")
    print(f"\n=== Fuzz {name} ===")
    stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3 - {exe}", timeout=600)
    out = stdout.read().decode()
    print(out)
    err = stderr.read().decode()
    if err: print(f"[STDERR] {err[:300]}")

c.close()
