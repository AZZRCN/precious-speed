#!/usr/bin/env python3
import paramiko, random, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

CMD_COMPILE = "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DDISABLE_2NXN_CYCLIC moptm_fusion.cpp -o moptm_debug -pthread"

DEBUG_SCRIPT = """
import random, subprocess, sys, json
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

fails = []
for i, (a_d, b_d) in enumerate(tests):
    inp, sa, sb = gen(a_d, b_d)
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r = subprocess.run(['bash', '-c', './moptm_debug < /tmp/fuzz.in'], capture_output=True, timeout=120)
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
    except:
        q_ok = False
        r_ok = False
        mq, mr = '', ''
    if not q_ok or not r_ok:
        fails.append({
            'idx': i,
            'a_d': a_d,
            'b_d': b_d,
            'q_ok': q_ok,
            'r_ok': r_ok,
        })

# 打印总结
print(f"Total fails: {len(fails)}")
for f in fails:
    print(f"  #{f['idx']}: a={f['a_d']} b={f['b_d']} q_ok={f['q_ok']} r_ok={f['r_ok']}")

import os
os.unlink('/tmp/fuzz.in')
"""
DEBUG_B64 = base64.b64encode(DEBUG_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded")

print("\n=== Compile ===")
stdin, stdout, stderr = c.exec_command(CMD_COMPILE, timeout=300)
rc = stdout.channel.recv_exit_status()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if err: print(f"[STDERR] {err}")
if rc != 0: exit(1)

print("\n=== Run ===")
stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {DEBUG_B64} | base64 -d | python3", timeout=3600)
print(stdout.read().decode())
err = stderr.read().decode()
if err: print(f"[STDERR] {err}")

c.close()
