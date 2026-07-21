#!/usr/bin/env python3
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
for _ in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

import sys
target = sys.argv[1] if len(sys.argv) > 1 else "moptm_test"

fail = 0
q_wrong = 0
r_wrong = 0
both_wrong = 0
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
        q_ok = (mq == sq)
        r_ok = (mr == sr)
        ok = q_ok and r_ok
    except:
        q_ok = False
        r_ok = False
        ok = False
    if not ok:
        fail += 1
        if not q_ok and not r_ok: both_wrong += 1
        elif not q_ok: q_wrong += 1
        else: r_wrong += 1
        if fail <= 10:
            print(f"FAIL #{i}: a={a_d} b={b_d} q_ok={q_ok} r_ok={r_ok}")
            sys.stdout.flush()

print(f"\\n=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ({target})")
print(f"    q_wrong={q_wrong} r_wrong={r_wrong} both_wrong={both_wrong}")
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

configs = [
    ("cyclic_only", ""),  # 不定义 DISABLE_2NXN_CYCLIC = 启用 cyclic
    ("gmp_cyclic", "-DUSE_GMP_NEWTON"),
    ("baseline", "-DDISABLE_2NXN_CYCLIC"),
]

for name, flags in configs:
    exe = f"moptm_{name}"
    cmd = f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV {flags} moptm_fusion.cpp -o {exe} -pthread"
    print(f"\n=== Compile {name} ===")
    print(f"  flags: {flags}")
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
