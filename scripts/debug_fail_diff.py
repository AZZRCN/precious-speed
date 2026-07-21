#!/usr/bin/env python3
"""详细对比 FAIL case 的实际输出, 找出差异位置"""
import paramiko, random, hashlib

HOST = "10.144.33.157"
USER = "azzr"
PWD = "1234"

# FAIL cases
FAIL_CASES = [
    (37, 50000, 10000),
    (98, 20000, 6664),
    (106, 2000, 664),
    (164, 500000, 250000),
]

def gen(a_digits, b_digits, seed_offset=0):
    rng = random.Random(2026 + seed_offset)
    # 复现 fuzz_carry_fix.py 的生成逻辑
    for _ in range(37):  # 跳到 #37
        a_d = rng.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
        b_d = a_d // rng.choice([2, 3, 4, 5])
        b_d = max(b_d, 10)
        b_d = (b_d // 4) * 4
        if b_d < 4: b_d = 4
    return None

# 更简单的方法: 直接在 VM 上生成 FAIL case 的输入
DIAG_SCRIPT = """
import random, subprocess, hashlib

random.seed(2026)
tests = []
for _ in range(200):
    a_d = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
    b_d = a_d // random.choice([2, 3, 4, 5])
    b_d = max(b_d, 10)
    b_d = (b_d // 4) * 4
    if b_d < 4: b_d = 4
    tests.append((a_d, b_d))

# 复现 FAIL case 的输入
FAIL_IDX = [37, 98, 106, 164]
for idx in FAIL_IDX:
    a_d, b_d = tests[idx]
    random.seed(2026 + idx)  # 用相同 seed 复现
    # 注意: fuzz_carry_fix.py 用的是全局 random, 不是 per-case seed
    # 所以需要重新跑一遍到 idx
    random.seed(2026)
    for i in range(idx + 1):
        a_d2 = random.choice([100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000])
        b_d2 = a_d2 // random.choice([2, 3, 4, 5])
        b_d2 = max(b_d2, 10)
        b_d2 = (b_d2 // 4) * 4
        if b_d2 < 4: b_d2 = 4
        if i == idx:
            a_d, b_d = a_d2, b_d2
            break
    # 生成 a, b (复现 fuzz_carry_fix.py 的 gen)
    a = ''.join([str(random.randint(0,9)) for _ in range(a_d)])
    b = ''.join([str(random.randint(1,9))] + [str(random.randint(0,9)) for _ in range(b_d-1)])
    inp = f"1\\n{a}\\n{b}\\n"
    with open(f'/tmp/fail_{idx}.in', 'w') as f:
        f.write(inp)
    r1 = subprocess.run(['bash', '-c', f'./moptm_default < /tmp/fail_{idx}.in'], capture_output=True, timeout=120)
    r2 = subprocess.run(['bash', '-c', f'./moptm_disable < /tmp/fail_{idx}.in'], capture_output=True, timeout=120)
    print(f"=== FAIL #{idx}: a={a_d} b={b_d} ===")
    print(f"  default len={len(r1.stdout)} disable len={len