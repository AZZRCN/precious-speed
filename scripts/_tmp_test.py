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

print(f"\\n=== {len(tests)-fail}/{len(tests)} PASS, {fail} FAIL ({target})")
os.unlink('/tmp/fuzz.in')
"""
FUZZ_B64 = base64.b64encode(FUZZ_SCRIPT.encode()).decode()

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)
print("[OK] SSH connected")

# 用 sed 在源码中加一个宏来禁用验证
with open("d:/precious_speed/moptm_fusion.cpp", "rb") as f:
    src = f.read().decode()

# 保存原版
with open("d:/precious_speed/moptm_fusion_orig.cpp", "wb") as f:
    f.write(src.encode())

# 注释掉验证逻辑
import re
# 找到 "// 验证: D * inv <= B^(2*in)" 到 "if (is_upper) {" 之间的代码
# 简单起见，我们直接用 sed 替换
c.close()

# 让我直接修改本地文件测试
# 先恢复原版，然后测试 baseline 和 gmp-only 对比 inv 输出
# 实际上，让我用更简单的方法：在 absDivMu 的 GMP 路径中，直接调用 absInvNewton 而不是 absInvNewtonGMP，看看是否还有 4 FAIL
# 如果没有，说明 absInvNewtonGMP 有 bug；如果有，说明验证逻辑有 bug

print("让我直接修改源码测试...")
