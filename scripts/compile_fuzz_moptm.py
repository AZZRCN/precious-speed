#!/usr/bin/env python3
"""Upload moptm_fusion.cpp, compile 3 variants (ADD/MUL/DIV), fuzz test correctness.
Usage: python scripts/compile_fuzz_moptm.py
"""
import paramiko, sys, base64, time

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

REMOTE_SCRIPT = r'''
import subprocess, os, sys, random, hashlib, time

WORK = "/tmp/bench_moptm"
os.makedirs(WORK, exist_ok=True)

# === Upload check ===
src = "/home/azzr/moptm_fusion.cpp"
if not os.path.exists(src):
    print(f"[FAIL] {src} not found")
    sys.exit(1)
print(f"[OK] src={src} ({os.path.getsize(src)} bytes)")

# === Compile 3 variants ===
# Mode switching: default HINT_OP_DIV; for ADD/MUL use -D + sed comment
def compile_variant(mode):
    exe = f"{WORK}/moptm_{mode}"
    if mode == "DIV":
        # Default mode (HINT_OP_DIV defined in file)
        cmd = (f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE "
               f"{src} -o {exe} -pthread 2>&1")
    else:
        # Need to comment out #define HINT_OP_DIV and add -DHINT_OP_{mode}
        tmp_src = f"{WORK}/moptm_{mode}.cpp"
        with open(src, 'r') as f:
            content = f.read()
        # Comment the default #define HINT_OP_DIV line
        content = content.replace("#define HINT_OP_DIV", "// #define HINT_OP_DIV")
        with open(tmp_src, 'w') as f:
            f.write(content)
        cmd = (f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE "
               f"-DHINT_OP_{mode} "
               f"{tmp_src} -o {exe} -pthread 2>&1")
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print(f"[FAIL] compile {mode}:\n{r.stdout[:3000]}")
        return None
    print(f"[OK] compiled moptm_{mode}")
    return exe

exes = {}
for mode in ["ADD", "MUL", "DIV"]:
    exe = compile_variant(mode)
    if exe is None:
        sys.exit(1)
    exes[mode] = exe

# === Compile best references (if not exist) ===
best_map = {"ADD": "/home/azzr/best/add.cpp",
            "MUL": "/home/azzr/best/mul.cpp",
            "DIV": "/home/azzr/best/div.cpp"}
best_exes = {}
for mode, bsrc in best_map.items():
    bexe = f"/home/azzr/best_{mode.lower()}"
    if not os.path.exists(bexe):
        if not os.path.exists(bsrc):
            print(f"[WARN] {bsrc} not found, skipping best ref for {mode}")
            continue
        r = subprocess.run(
            f"g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE {bsrc} -o {bexe} -pthread 2>&1",
            shell=True, capture_output=True, text=True
        )
        if r.returncode != 0:
            print(f"[FAIL] compile best {mode}:\n{r.stdout[:2000]}")
            continue
        print(f"[OK] compiled best_{mode.lower()}")
    best_exes[mode] = bexe

# === Fuzz test data generation ===
def gen_add_case(seed):
    random.seed(seed)
    # Mix of lengths: small (1-18 digits) and large (100-10000 digits)
    choice = random.randint(0, 3)
    if choice == 0:
        # Small (int64 range)
        a = str(random.randint(0, 10**random.randint(1, 18) - 1))
        b = str(random.randint(0, 10**random.randint(1, 18) - 1))
    elif choice == 1:
        # Medium (100-1000 digits)
        la = random.randint(100, 1000)
        lb = random.randint(100, 1000)
        a = ''.join([str(random.randint(0,9)) for _ in range(la)])
        b = ''.join([str(random.randint(0,9)) for _ in range(lb)])
        a = a.lstrip('0') or '0'
        b = b.lstrip('0') or '0'
    else:
        # Large (1000-10000 digits)
        la = random.randint(1000, 10000)
        lb = random.randint(1000, 10000)
        a = ''.join([str(random.randint(0,9)) for _ in range(la)])
        b = ''.join([str(random.randint(0,9)) for _ in range(lb)])
        a = a.lstrip('0') or '0'
        b = b.lstrip('0') or '0'
    return f"{a} {b}\n"

def gen_mul_case(seed):
    return gen_add_case(seed)  # same format: a b

def gen_div_case(seed):
    random.seed(seed)
    choice = random.randint(0, 3)
    if choice == 0:
        a = str(random.randint(0, 10**random.randint(1, 18) - 1))
        b = str(random.randint(1, 10**random.randint(1, 18) - 1))  # b >= 1
    elif choice == 1:
        la = random.randint(100, 1000)
        lb = random.randint(1, 100)  # divisor shorter
        a = ''.join([str(random.randint(0,9)) for _ in range(la)])
        b = ''.join([str(random.randint(0,9)) for _ in range(lb)])
        a = a.lstrip('0') or '0'
        b = b.lstrip('0') or '0'
        if b == '0': b = '1'
    else:
        la = random.randint(1000, 5000)
        lb = random.randint(100, la // 2)  # divisor <= dividend/2
        a = ''.join([str(random.randint(0,9)) for _ in range(la)])
        b = ''.join([str(random.randint(0,9)) for _ in range(lb)])
        a = a.lstrip('0') or '0'
        b = b.lstrip('0') or '0'
        if b == '0': b = '1'
    return f"{a} {b}\n"

gen_map = {"ADD": gen_add_case, "MUL": gen_mul_case, "DIV": gen_div_case}
mode_int = lambda m: {"ADD": 0, "MUL": 1000, "DIV": 2000}[m]

# === Fuzz test ===
print("\n=== Fuzz test (200 cases per mode) ===")
all_pass = True
FUZZ_COUNT = 200

for mode in ["ADD", "MUL", "DIV"]:
    gen_fn = gen_map[mode]
    lines = [str(FUZZ_COUNT)]
    for i in range(FUZZ_COUNT):
        lines.append(gen_fn(i + mode_int(mode)).strip())
    inp = '\n'.join(lines) + '\n'
    inp_file = f"{WORK}/fuzz_{mode}.in"
    with open(inp_file, 'w') as f:
        f.write(inp)

    # Run moptm
    out_moptm = f"{WORK}/fuzz_{mode}_moptm.out"
    r = subprocess.run(f"{exes[mode]} < {inp_file} > {out_moptm}", shell=True)
    if r.returncode != 0:
        print(f"  {mode}: moptm RUNTIME ERROR (exit {r.returncode})")
        all_pass = False
        continue

    if mode in best_exes:
        out_best = f"{WORK}/fuzz_{mode}_best.out"
        r = subprocess.run(f"{best_exes[mode]} < {inp_file} > {out_best}", shell=True)
        if r.returncode != 0:
            print(f"  {mode}: best RUNTIME ERROR (exit {r.returncode})")
            all_pass = False
            continue
        with open(out_moptm, 'rb') as f: md5_m = hashlib.md5(f.read()).hexdigest()
        with open(out_best, 'rb') as f: md5_b = hashlib.md5(f.read()).hexdigest()
        ok = "PASS" if md5_m == md5_b else "FAIL"
        if md5_m != md5_b:
            all_pass = False
            print(f"  {mode}: moptm={md5_m[:12]} best={md5_b[:12]} [{ok}]")
            # Show first diff
            with open(out_moptm, 'r') as f: lm = f.readlines()
            with open(out_best, 'r') as f: lb = f.readlines()
            for i, (a, b) in enumerate(zip(lm, lb)):
                if a != b:
                    print(f"    line {i}: moptm={a[:60].strip()} vs best={b[:60].strip()}")
                    break
        else:
            print(f"  {mode}: {md5_m[:12]} [{ok}] ({FUZZ_COUNT} cases)")
    else:
        # No best ref, just check it ran
        print(f"  {mode}: ran OK ({FUZZ_COUNT} cases, no best ref)")

if not all_pass:
    print("\n[FAIL] Correctness check failed!")
    sys.exit(1)
print("\n[OK] All fuzz tests passed")
'''

B64 = base64.b64encode(REMOTE_SCRIPT.encode()).decode()

print("=== moptm_fusion compile + fuzz ===")
print()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print(f"[OK] SSH connected to {HOST}")

sftp = c.open_sftp()
sftp.put("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")
sftp.close()
print("[OK] Uploaded moptm_fusion.cpp")

# Upload best files if missing
for mode, local in [("ADD", "d:/precious_speed/best/add.cpp"),
                     ("MUL", "d:/precious_speed/best/mul.cpp"),
                     ("DIV", "d:/precious_speed/best/div.cpp")]:
    remote = f"/home/azzr/best/{mode.lower()}.cpp"
    stdin, stdout, stderr = c.exec_command(f"mkdir -p /home/azzr/best && test -f {remote} && echo EXISTS || echo MISSING")
    if stdout.read().decode().strip() == "MISSING":
        sftp = c.open_sftp()
        sftp.put(local, remote)
        sftp.close()
        print(f"[OK] Uploaded best/{mode.lower()}.cpp")

print("\n=== Running compile + fuzz ===")
stdin, stdout, stderr = c.exec_command(
    f"cd /home/azzr && echo {B64} | base64 -d | python3 -u",
    timeout=300
)
out = stdout.read().decode()
print(out)
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:1000]}")

c.close()
print("[DONE]")
