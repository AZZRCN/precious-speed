#!/usr/bin/env python3
"""Debug: verify if post-verification triggers for FAIL cases."""
import paramiko, base64, random

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# FAIL cases
FAIL_CASES = [
    (50000, 10000),   # #37
    (20000, 6664),    # #98
    (2000, 664),      # #106
    (500000, 250000), # #164
]

def gen(a_digits, b_digits, rng):
    a = ''.join([str(rng.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(rng.randint(1,9))] + [str(rng.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\n{a}\n{b}\n"

def gen_all_inputs():
    rng = random.Random(2026)
    tests = []
    for _ in range(200):
        a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
        b_d = a_d // random.choice([2, 3, 4, 5])
        b_d = max(b_d, 10)
        b_d = (b_d // 4) * 4
        if b_d < 4: b_d = 4
        tests.append((a_d, b_d))
    inputs = []
    for a_d, b_d in tests:
        inputs.append(gen(a_d, b_d, rng))
    return inputs, tests

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

print("=== Upload ===")
sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
print("[OK]")

# Compile with DEBUG_INV_VERIFY + USE_GMP_NEWTON
print("\n=== Compile (DEBUG_INV_VERIFY + USE_GMP_NEWTON) ===")
cmd = "cd /home/azzr && g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DUSE_GMP_NEWTON -DDEBUG_INV_VERIFY moptm_fusion.cpp -o moptm_debug -pthread 2>&1"
stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
rc = stdout.channel.recv_exit_status()
out = stdout.read().decode()
err = stderr.read().decode()
print(f"[EXIT {rc}]")
if out: print(f"[STDOUT] {out[:500]}")
if err: print(f"[STDERR] {err[:500]}")
if rc != 0:
    sftp.close()
    c.close()
    exit(1)

# Generate all 200 inputs
inputs, tests = gen_all_inputs()

# Run only the FAIL cases
print("\n=== Run FAIL cases with debug output ===")
for idx in [37, 98, 106, 164]:
    a_d, b_d = tests[idx]
    inp = inputs[idx]
    with sftp.open(f"/home/azzr/fuzz_{idx}.in", "w") as f:
        f.write(inp)
    cmd = f"cd /home/azzr && ./moptm_debug < fuzz_{idx}.in > /dev/null 2> /tmp/dbg_{idx}.txt; head -20 /tmp/dbg_{idx}.txt"
    stdin, stdout, stderr = c.exec_command(cmd, timeout=120)
    out = stdout.read().decode()
    print(f"\n--- #{idx} (a={a_d}, b={b_d}) ---")
    print(out)

sftp.close()
c.close()
print("[DONE]")
