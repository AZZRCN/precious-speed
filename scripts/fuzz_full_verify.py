#!/usr/bin/env python3
# 全面验证: 4 个编译配置 vs Python 正确答案
import paramiko, base64, sys

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

CONFIGS = [
    ("full",     "-DUSE_GMP_NEWTON"),
    ("nocyclic", "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC"),
    ("nogmp",    "-DDISABLE_2NXN_CYCLIC"),
    ("gmp_cyclic", ""),  # 都启用？不对，都启用是 full
]
# 修正: 4 个组合
CONFIGS = [
    ("gmp+cyclic",  "-DUSE_GMP_NEWTON"),
    ("gmp-only",    "-DUSE_GMP_NEWTON -DDISABLE_2NXN_CYCLIC"),
    ("cyclic-only", ""),
    ("baseline",    "-DDISABLE_2NXN_CYCLIC"),
]

FUZZ_SCRIPT = """
import random, subprocess, hashlib, os, sys
sys.set_int_max_str_digits(2000000)
random.seed(2026)

def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\\n{a}\\n{b}\\n", a, b

tests = []
for _ in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

import sys
target = sys.argv[1] if len(sys.argv) > 1 else "moptm_full"

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
        print(f"FAIL #{i}: a={a_d} b={b_d}")
        sys.stdout.flush()
    elif i % 50 == 0:
        print(f"OK #{i}: a={a_d} b={b_d}")
        sys.stdout.flush()

print(f"\\n=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ({target}) ===")
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

for name, flags in CONFIGS:
    exe = f"moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread"
    print(f"\n=== Compile {name} ===")
    print(f"  flags: {flags}")
    stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
    rc = stdout.channel.recv_exit_status()
    err = stderr.read().decode()
    print(f"  [EXIT {rc}]")
    if err: print(f"  [STDERR] {err[:200]}")
    if rc != 0:
        print(f"  COMPILE FAILED, skipping")
        continue

    print(f"\n=== Fuzz {name} ===")
    stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3 - {exe}", timeout=3600)
    out = stdout.read().decode()
    print(out)
    err = stderr.read().decode()
    if err: print(f"[STDERR] {err[:200]}")

c.close()
