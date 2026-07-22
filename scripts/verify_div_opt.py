#!/usr/bin/env python3
"""Verify div.cpp (inv precision optimization) with g++ 11 on VM.
Tests: burnikel_00..03 + regular DIV correctness + fuzz + performance benchmark."""
import paramiko, base64, time

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

SCRIPT = r'''
import subprocess, sys, random, time
sys.set_int_max_str_digits(0)

SUM_OF_CHARACTER_LENGTH = 4000002

# --- Find g++ 11 ---
print("=== Locating g++ 11 ===")
for cand in ["g++-11", "/usr/bin/g++-11", "g++-11.5", "/usr/local/bin/g++-11"]:
    r = subprocess.run(f"which {cand} 2>/dev/null && {cand} --version | head -1",
                      shell=True, capture_output=True, text=True)
    if r.returncode == 0:
        GXX11 = cand
        print(f"[OK] Found: {r.stdout.strip()}")
        break
else:
    r = subprocess.run("ls /usr/bin/g++-* 2>/dev/null", shell=True, capture_output=True, text=True)
    print(f"[INFO] g++ variants: {r.stdout.strip()}")
    if "/usr/bin/g++-11" in r.stdout:
        GXX11 = "/usr/bin/g++-11"
        print(f"[OK] Using: {GXX11}")
    else:
        print("[FAIL] No g++ 11 found")
        sys.exit(1)

# --- Generate burnikel inputs ---
print("\n=== Generating burnikel inputs ===")
def gen_burnikel(seed):
    base = [10, 1000000000, 2, 1 << 32][seed % 4]
    bb = base
    A, B = [], []
    lsum = 0
    while True:
        quotient = 2 * bb * bb - 1
        remainder = (bb - 1) * bb
        y = remainder + 1
        x = quotient * y + remainder
        sa, sb = str(x), str(y)
        lsum += len(sa) + len(sb)
        if lsum > SUM_OF_CHARACTER_LENGTH:
            break
        A.append(sa); B.append(sb)
        bb *= base
    out = [str(len(A))]
    for a, b in zip(A, B):
        out.append(f"{a} {b}")
    return "\n".join(out) + "\n"

for seed in [0, 1, 2, 3]:
    inp = gen_burnikel(seed)
    with open(f"/tmp/burnikel_{seed:02d}.in", "w") as f:
        f.write(inp)
    ncases = inp.strip().split("\n")
    print(f"  seed={seed}: {len(ncases)-1} cases, {len(inp)} bytes")

# --- Compile with g++ 11 (LC env) ---
print(f"\n=== Compiling div.cpp with {GXX11} ===")
r = subprocess.run(
    f"{GXX11} -O2 -std=gnu++20 -static -DONLINE_JUDGE "
    "/home/azzr/div.cpp -o /home/azzr/div_g11 -pthread 2>&1",
    shell=True, capture_output=True, text=True, timeout=120)
if r.returncode != 0:
    print(f"COMPILE FAIL:\n{r.stdout[:3000]}")
    sys.exit(1)
print("[OK] div_g11 compiled")

# --- Burnikel tests ---
print("\n=== Burnikel tests (exit code only) ===")
burnikel_pass = True
for s in ["00", "01", "02", "03"]:
    inp = f"/tmp/burnikel_{s}.in"
    r = subprocess.run(f'/home/azzr/div_g11 < {inp}',
                      shell=True, capture_output=True, text=True, timeout=120)
    status = "PASS" if r.returncode == 0 else f"RE(exit={r.returncode})"
    if r.returncode != 0:
        burnikel_pass = False
    err = r.stderr[:300] if r.stderr else ""
    print(f"  burnikel_{s}: {status}  {err}")
print(f"\n[Burnikel] {'ALL PASS' if burnikel_pass else 'FAILED'}")

# --- Regular DIV correctness ---
print("\n=== Regular DIV tests (Python verify) ===")
div_pass = True
for da, db, label in [(1000000, 500000, "1M_500k"), (200000, 100000, "200k_100k"), (10000, 10000, "10k")]:
    random.seed(42)
    a = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(da-1)])
    b = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(db-1)])
    inp = f"1\n{a} {b}\n"
    with open(f'/tmp/div_{label}.in', 'w') as f:
        f.write(inp)
    r = subprocess.run(f'/home/azzr/div_g11 < /tmp/div_{label}.in > /tmp/div_{label}.out',
                      shell=True, capture_output=True, text=True, timeout=60)
    status = "PASS" if r.returncode == 0 else f"RE(exit={r.returncode})"
    err = r.stderr[:200] if r.stderr else ""
    verify = ""
    if r.returncode == 0:
        ai, bi = int(a), int(b)
        eq, er = ai // bi, ai % bi
        with open(f'/tmp/div_{label}.out') as f:
            out = f.read().strip().split()
        if len(out) >= 2:
            q_ok = out[0] == str(eq)
            r_ok = out[1] == str(er)
            verify = f" q={'OK' if q_ok else 'FAIL'} r={'OK' if r_ok else 'FAIL'}"
            if not q_ok: verify += f" (exp_q_len={len(str(eq))} got={len(out[0])})"
            if not r_ok: verify += f" (exp_r_len={len(str(er))} got={len(out[1])})"
            if not q_ok or not r_ok:
                div_pass = False
        else:
            verify = " OUTPUT_TOO_SHORT"
            div_pass = False
    else:
        div_pass = False
    print(f"  {label}: {status}{verify}  {err}")
print(f"\n[Regular DIV] {'ALL PASS' if div_pass else 'FAILED'}")

# --- Fuzz test ---
print("\n=== Fuzz test (200 cases) ===")
fuzz_pass = True
fuzz_fail = 0
random.seed(12345)
for i in range(200):
    da = random.randint(1, 5000)
    db = random.randint(1, min(da, 5000))
    a = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(da-1)]) if da > 1 else str(random.randint(1, 9))
    b = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(db-1)]) if db > 1 else str(random.randint(1, 9))
    inp = f"1\n{a} {b}\n"
    with open('/tmp/fuzz.in', 'w') as f:
        f.write(inp)
    r = subprocess.run('/home/azzr/div_g11 < /tmp/fuzz.in > /tmp/fuzz.out',
                      shell=True, capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        fuzz_pass = False
        fuzz_fail += 1
        if fuzz_fail <= 3:
            print(f"  case {i}: RE(exit={r.returncode}) da={da} db={db}")
        continue
    ai, bi = int(a), int(b)
    eq, er = ai // bi, ai % bi
    with open('/tmp/fuzz.out') as f:
        out = f.read().strip().split()
    if len(out) < 2 or out[0] != str(eq) or out[1] != str(er):
        fuzz_pass = False
        fuzz_fail += 1
        if fuzz_fail <= 3:
            print(f"  case {i}: WRONG da={da} db={db} q_ok={out[0]==str(eq) if len(out)>=1 else False} r_ok={out[1]==str(er) if len(out)>=2 else False}")
print(f"  {'ALL PASS (200/200)' if fuzz_pass else f'FAILED ({fuzz_fail}/200 failed)'}")

# --- Performance benchmark (5 runs, take min) ---
print("\n=== Performance benchmark (5 runs, min) ===")
for da, db, label in [(1000000, 500000, "1M_500k"), (500000, 500000, "500k_500k"), (200000, 100000, "200k_100k")]:
    random.seed(42)
    a = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(da-1)])
    b = str(random.randint(1, 9)) + ''.join([str(random.randint(0,9)) for _ in range(db-1)])
    inp = f"1\n{a} {b}\n"
    with open(f'/tmp/bench_{label}.in', 'w') as f:
        f.write(inp)
    times = []
    for _ in range(5):
        t0 = time.time()
        r = subprocess.run(f'/home/azzr/div_g11 < /tmp/bench_{label}.in > /dev/null',
                          shell=True, capture_output=True, text=True, timeout=60)
        dt = (time.time() - t0) * 1000
        if r.returncode == 0:
            times.append(dt)
    if times:
        print(f"  {label}: min={min(times):.1f}ms avg={sum(times)/len(times):.1f}ms")
    else:
        print(f"  {label}: ALL FAILED")

print("\n=== SUMMARY ===")
print(f"  Burnikel: {'PASS' if burnikel_pass else 'FAIL'}")
print(f"  Regular DIV: {'PASS' if div_pass else 'FAIL'}")
print(f"  Fuzz: {'PASS' if fuzz_pass else 'FAIL'}")
print(f"  Overall: {'READY FOR LC' if (burnikel_pass and div_pass and fuzz_pass) else 'NOT READY'}")
print("[DONE]")
'''

B64 = base64.b64encode(SCRIPT.encode()).decode()
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)

sftp = c.open_sftp()
sftp.put("d:/precious_speed/div.cpp", "/home/azzr/div.cpp")
sftp.close()
print("[OK] Uploaded div.cpp\n")

stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {B64} | base64 -d | python3 -u", timeout=600)
print(stdout.read().decode())
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:500]}")
c.close()
print("[DONE]")
