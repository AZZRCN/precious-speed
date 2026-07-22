#!/usr/bin/env python3
"""大规模验证 cyclic Bug #2 最终修复 (方案9d + 移除±1调整)

3000 cases, 不同随机种子, 覆盖更多边界用例
- 600 large (1M/500k/200k etc)
- 300 power-of-2 aligned
- 500 medium
- 500 small
- 300 very small
- 500 extreme ratio (a>>b) — 关键测试 cyclic_m fix
- 300 near-boundary (remainder near 0 or divisor)
"""
import paramiko, base64, sys, time

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# === Fuzz script (runs on VM) ===
FUZZ_SCRIPT = r"""
import random, subprocess, os, sys, time
sys.set_int_max_str_digits(2000000)
random.seed(2026072202)

def gen(a_digits, b_digits):
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\n{a}\n{b}\n", a, b

target = sys.argv[1]

tests = []

# 600 Large cases (where cyclic path triggers)
for _ in range(600):
    a_d = random.choice([10000, 20000, 50000, 100000, 200000, 500000, 1000000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 300 Power-of-2 aligned cases
for _ in range(300):
    k = random.choice([4096, 8192, 16384, 32768, 65536, 131072])
    a_d = k * random.choice([2, 3, 5, 7])
    b_d = k
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 500 Medium cases
for _ in range(500):
    a_d = random.choice([50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3])
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 500 Small cases
for _ in range(500):
    a_d = random.choice([100, 500, 1000, 5000, 8000])
    b_d = a_d // random.choice([2, 3])
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 300 Very small cases
for _ in range(300):
    a_d = random.choice([4, 8, 12, 16, 20, 40, 80])
    b_d = random.choice([4, 8, 12])
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    if a_d < b_d: a_d, b_d = b_d, a_d
    tests.append((a_d, b_d))

# 500 Extreme ratio cases (a >> b) — KEY test for cyclic_m fix
for _ in range(500):
    a_d = random.choice([100000, 500000, 1000000])
    b_d = random.choice([10, 100, 1000, 10000])
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 300 Near-boundary cases (remainder close to 0 or divisor)
for _ in range(300):
    a_d = random.choice([10000, 50000, 100000, 500000])
    b_d = a_d // 2
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d, "r_near_zero"))

random.shuffle(tests)

fail = 0
total = len(tests)
fail_details = []
t0 = time.time()

for i, t in enumerate(tests):
    if len(t) == 3:
        a_d, b_d, tag = t
        if tag == "r_near_zero":
            a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
            b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
            A_int = int(a)
            B_int = int(b)
            Q = A_int // B_int
            A_int = B_int * Q + 1
            sa = str(A_int)
            sb = b
            inp = f"1\n{sa}\n{sb}\n"
        else:
            inp, sa, sb = gen(a_d, b_d)
    else:
        a_d, b_d = t
        inp, sa, sb = gen(a_d, b_d)

    with open('/tmp/fuzz_large.in', 'w') as f:
        f.write(inp)
    try:
        r = subprocess.run([target], stdin=open('/tmp/fuzz_large.in'),
                           capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        fail += 1
        fail_details.append(f"FAIL #{i}: TIMEOUT a={a_d} b={b_d}")
        continue

    A = int(sa)
    B = int(sb)
    Q = A // B
    R = A % B
    sq, sr = str(Q), str(R)
    try:
        line_out = r.stdout.decode().strip()
        parts_out = line_out.split(' ')
        mq, mr = parts_out[0], parts_out[1]
        ok = (mq == sq and mr == sr)
    except:
        ok = False

    if not ok:
        fail += 1
        qdiff = ""
        try:
            if len(mq) > 20 and len(sq) > 20:
                for j in range(min(len(mq), len(sq))):
                    if mq[j] != sq[j]:
                        qdiff = f" Q_diff@{j}:got={mq[j]} exp={sq[j]}"
                        break
                else:
                    mlen = min(len(mq), len(sq))
                    if len(mq) > len(sq):
                        qdiff = f" Q_longer_by={len(mq)-len(sq)}"
                    elif len(sq) > len(mq):
                        qdiff = f" Q_shorter_by={len(sq)-len(mq)}"
        except:
            pass
        detail = f"FAIL #{i}: a={a_d} b={b_d}{qdiff}"
        fail_details.append(detail)
        if fail <= 30:
            print(detail)
            sys.stdout.flush()

    if (i + 1) % 200 == 0:
        elapsed = time.time() - t0
        eta = elapsed / (i + 1) * (total - i - 1)
        print(f"  progress: {i+1}/{total}, fail={fail}, elapsed={elapsed:.0f}s, eta={eta:.0f}s")
        sys.stdout.flush()

elapsed = time.time() - t0
print(f"\n=== {total-fail}/{total} PASS, {fail} FAIL ({target}) elapsed={elapsed:.0f}s")
if fail_details:
    print(f"\nFirst 30 failures:")
    for d in fail_details[:30]:
        print(f"  {d}")
os.unlink('/tmp/fuzz_large.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

# === Benchmark script (runs on VM) ===
BENCH_SCRIPT = r"""
import subprocess, time, os, sys
sys.set_int_max_str_digits(2000000)

target = sys.argv[1]

import random
random.seed(42)
a = ''.join([str(random.randint(0,9)) for _ in range(1000000)])
b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(499999)])
with open('/tmp/bench_div.in', 'w') as f:
    f.write(f"1\n{a}\n{b}\n")

for _ in range(2):
    subprocess.run([target], stdin=open('/tmp/bench_div.in'),
                   capture_output=True, timeout=60)

times = []
for _ in range(7):
    t0 = time.perf_counter()
    subprocess.run([target], stdin=open('/tmp/bench_div.in'),
                   capture_output=True, timeout=60)
    t1 = time.perf_counter()
    times.append((t1 - t0) * 1000)

times.sort()
median = times[len(times)//2]
print(f"DIV 1M/500k median: {median:.2f} ms (7 runs: {', '.join(f'{t:.2f}' for t in times)})")
os.unlink('/tmp/bench_div.in')
"""
BENCH_B64 = base64.b64encode(BENCH_SCRIPT.encode()).decode()

# === Main ===
print("=== Large-Scale Verification: cyclic Bug #2 Final Fix (3000 cases) ===")
print(f"Config: scheme 9d + remove +/-1 adjustment")
print(f"Seed: 2026072202 (different from 1000-case run)")
print()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded moptm_fusion.cpp")

# Compile
exe = "moptm_verify"
cmd = f"cd /home/azzr && g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE moptm_fusion.cpp -o {exe} -pthread 2>&1"
print(f"\n=== Compile ===")
stdin, stdout, stderr = c.exec_command(cmd, timeout=300)
rc = stdout.channel.recv_exit_status()
out = stdout.read().decode()
if rc != 0:
    print(f"  COMPILE FAILED (rc={rc}):")
    print(out[:3000])
    c.close()
    sys.exit(1)
print(f"  [OK] compiled {exe}")

# Run fuzz
print(f"\n=== Fuzz (3000 cases, seed=2026072202) ===")
stdin, stdout, stderr = c.exec_command(
    f"cd /home/azzr && echo {FUZZ_B64} | base64 -d | python3 -u - ./{exe}",
    timeout=7200
)
out = stdout.read().decode()
print(out)
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:500]}")

all_pass = ", 0 FAIL" in out
if all_pass:
    print("\n=== All 3000 PASS! Running benchmark ===")
    stdin, stdout, stderr = c.exec_command(
        f"cd /home/azzr && echo {BENCH_B64} | base64 -d | python3 -u - ./{exe}",
        timeout=120
    )
    bench_out = stdout.read().decode()
    print(bench_out)
    bench_err = stderr.read().decode()
    if bench_err:
        print(f"[BENCH STDERR] {bench_err[:500]}")
else:
    print("\n=== FAILURES detected! Analyzing... ===")

c.close()
print("[DONE]")
