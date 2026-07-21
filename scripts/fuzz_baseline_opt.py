#!/usr/bin/env python3
"""大规模 fuzz 测试: 验证自适应 mu_in + Core1 fast 路径"""
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh

ssh.upload("d:/precious_speed/moptm_fusion.cpp", "/home/azzr/moptm_fusion.cpp")

# 编译
print("=== Compile ===")
exe, rc, err = ssh.upload_and_compile(
    "d:/precious_speed/moptm_fusion.cpp",
    "/home/azzr/moptm_fusion.cpp",
    "-DDISABLE_2NXN_CYCLIC"
)
if rc != 0:
    print(f"FAIL:\n{err[:300]}")
    sys.exit(1)
print(f"OK: {exe}")

# 大规模 fuzz 测试 (在 VM 上运行)
FUZZ_SCRIPT = r"""
import random, subprocess, sys
sys.set_int_max_str_digits(2000000)

exe = "/home/azzr/moptm_fusion"
random.seed(2026)
fails = 0
total = 0

# 测试用例类型:
# 1. a >> b (大商, 触发 absDivMu)
# 2. a ≈ 2b (触发 absDivMu 或 Core2)
# 3. a < 2b (触发 Core1)
# 4. a ≈ b (触发 Core1, 小商)
# 5. 小尺寸

test_sizes = [
    # (a_digits, b_digits, description)
    (500000, 250000, "large 2:1"),    # absDivMu
    (100000, 50000, "med 2:1"),       # absDivMu
    (50000, 25000, "small 2:1"),      # absDivMu
    (10000, 5000, "tiny 2:1"),        # absDivMu
    (500000, 400000, "large 5:4"),    # Core1 (a < 2b)
    (100000, 80000, "med 5:4"),       # Core1
    (50000, 40000, "small 5:4"),      # Core1
    (10000, 8000, "tiny 5:4"),        # Core1
    (500000, 499000, "large ~1:1"),   # Core1 (a ≈ b)
    (100000, 99000, "med ~1:1"),      # Core1
    (50000, 49000, "small ~1:1"),     # Core1
    (10000, 9000, "tiny ~1:1"),       # Core1
    (100000, 30000, "med 10:3"),      # absDivMu (3:1 ratio)
    (100000, 10001, "med 10:1"),      # absDivMu (high ratio)
    (500000, 100000, "large 5:1"),    # absDivMu (high ratio)
    (1000, 500, "micro 2:1"),         # small
    (100, 50, "nano 2:1"),            # very small
    (10, 5, "pico 2:1"),              # minimal
    (3, 2, "minimal"),                # 3/2
    (1, 1, "single digit"),           # 1/1
]

for test_idx, (a_d, b_d, desc) in enumerate(test_sizes):
    for trial in range(10):
        total += 1
        random.seed(test_idx * 100 + trial)
        a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
        if a_d == 1:
            a = str(random.randint(1, 9))
        b = str(random.randint(1,9))
        if b_d > 1:
            b += ''.join([str(random.randint(0,9)) for _ in range(b_d-1)])
        
        A = int(a)
        B = int(b)
        Q = str(A // B)
        R = str(A % B)
        
        inp = f"1\n{a}\n{b}\n"
        with open("/tmp/fuzz.txt", "w") as f:
            f.write(inp)
        
        r = subprocess.run([exe], stdin=open("/tmp/fuzz.txt"),
                          capture_output=True, text=True, timeout=120)
        out = r.stdout.strip()
        parts = out.split(" ")
        
        if len(parts) != 2 or parts[0] != Q or parts[1] != R:
            fails += 1
            print(f"FAIL [{desc}] trial {trial}: a={a_d}d b={b_d}d")
            if len(parts) == 2:
                q_ok = parts[0] == Q
                r_ok = parts[1] == R
                print(f"  q_ok={q_ok}, r_ok={r_ok}")
                if not q_ok:
                    print(f"  Q expected len={len(Q)}, got len={len(parts[0])}")
                    print(f"  Q diff at: {next((i for i,(x,y) in enumerate(zip(Q,parts[0])) if x!=y), 'same')}")
                if not r_ok:
                    print(f"  R expected len={len(R)}, got len={len(parts[1])}")
            else:
                print(f"  Output: {out[:100]}")

print(f"\n{'='*50}")
print(f"Result: {total-fails}/{total} PASS")
if fails == 0:
    print("ALL PASS!")
"""

# 上传并运行 fuzz 脚本
import base64
b64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()
cmd = f"echo {b64} | base64 -d > /tmp/fuzz_script.py && python3 /tmp/fuzz_script.py"
rc, out, err = ssh.run(cmd, timeout=600)
print(out)
if err:
    print(f"STDERR: {err[:500]}")

print("Done!")
