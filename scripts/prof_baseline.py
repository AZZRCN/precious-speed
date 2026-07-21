#!/usr/bin/env python3
"""用 PROFILE_DIV 做 baseline 除法 profiling"""
import sys
sys.path.insert(0, "d:/precious_speed")
from ssh_manager import ssh
import random, base64, time

print("=== Upload + Compile (PROFILE_DIV) ===")
exe, rc, err = ssh.upload_and_compile(
    "d:/precious_speed/moptm_fusion.cpp",
    "/home/azzr/moptm_prof.cpp",
    "-DDISABLE_2NXN_CYCLIC -DPROFILE_DIV"
)
if rc != 0:
    print(f"Compile FAILED:\n{err[:500]}")
    sys.exit(1)
print(f"OK: {exe}")

def gen_case(a_digits, b_digits, seed=42):
    random.seed(seed)
    a = ''.join([str(random.randint(0,9)) for _ in range(a_digits)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_digits-1)])
    return f"1\n{a}\n{b}\n"

test_cases = [
    ("50k/25k",   50000,  25000),
    ("100k/50k", 100000,  50000),
    ("200k/100k",200000, 100000),
]

for name, a_d, b_d in test_cases:
    print(f"\n=== {name} ===")
    test_input = gen_case(a_d, b_d)
    b64 = base64.b64encode(test_input.encode()).decode()
    # 运行程序，prof 输出到 prof_detail.log
    cmd = f"cd /home/azzr && echo {b64} | base64 -d > /tmp/prof.in && {exe} < /tmp/prof.in > /dev/null 2>&1"
    rc, out, err = ssh.run(cmd, timeout=120)
    if rc != 0:
        print(f"  RUN FAILED (rc={rc}): {err[:200]}")
        continue
    # 读取 prof_detail.log
    rc, prof_out, _ = ssh.run("cat /home/azzr/prof_detail.log 2>/dev/null")
    if prof_out:
        print(prof_out[:3000])
    else:
        print("  (no prof output)")
    # 清理
    ssh.run("rm -f /home/azzr/prof_detail.log")

print("\nDone!")
