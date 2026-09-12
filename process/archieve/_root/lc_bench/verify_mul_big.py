import subprocess
import os
import sys

CUR = r"d:\precious_speed\lc_bench\exe\cur_mul_final.exe"
V0  = r"d:\precious_speed\lc_bench\exe\cur_mul_base.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

# 大用例验证：对比优化前后输出是否一致
big_cases = [
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "fft_killer_00", "fft_killer_01", "zero_00",
]

passed = 0
failed = 0
for c in big_cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"[SKIP] {c}: input not found")
        continue

    with open(inp, "rb") as f:
        r1 = subprocess.run([CUR], stdin=f, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, timeout=60)
    with open(inp, "rb") as f:
        r2 = subprocess.run([V0], stdin=f, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, timeout=60)

    if r1.stdout == r2.stdout and r1.returncode == 0 and r2.returncode == 0:
        print(f"[PASS] {c}  (out={len(r1.stdout)} bytes)")
        passed += 1
    else:
        print(f"[FAIL] {c}  cur_rc={r1.returncode} v0_rc={r2.returncode} "
              f"cur_len={len(r1.stdout)} v0_len={len(r2.stdout)}")
        failed += 1

print(f"\nTotal: {passed+failed}, Passed: {passed}, Failed: {failed}")
