#!/usr/bin/env python3
# 逐个测试 div_fuzz_big.in 的每个用例, 找出 cyclic 崩溃的用例
import subprocess
import sys

with open('div_fuzz_big.in', 'r') as f:
    lines = f.read().split('\n')

n = int(lines[0])
print(f"Total {n} cases")
for i in range(n):
    a = lines[1 + i*2]
    b = lines[2 + i*2]
    case_input = f"1\n{a}\n{b}\n"
    with open('_single.in', 'w') as sf:
        sf.write(case_input)
    r = subprocess.run(['test_cyclic.exe'], stdin=open('_single.in'), capture_output=True, timeout=60)
    if r.returncode != 0 or len(r.stdout) == 0:
        print(f"Case {i+1}: FAIL (rc={r.returncode}, stdout_len={len(r.stdout)}, stderr={r.stderr[:200]})")
        print(f"  a_digits={len(a)}, b_digits={len(b)}")
        # 也测 baseline
        r2 = subprocess.run(['test_baseline.exe'], stdin=open('_single.in'), capture_output=True, timeout=60)
        print(f"  baseline: rc={r2.returncode}, stdout_len={len(r2.stdout)}")
        break
    else:
        print(f"Case {i+1}: OK (a={len(a)}, b={len(b)}, stdout={len(r.stdout)} bytes)")
