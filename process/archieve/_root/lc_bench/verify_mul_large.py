#!/usr/bin/env python3
"""verify_mul_large.py - Compare cur_mul.exe vs mul_ref.exe (PROFILE_MAIN path) on large cases."""
import subprocess
import os
import sys

CUR = r"d:\precious_speed\cur_mul.exe"
REF = r"d:\precious_speed\lc_bench\exe\mul_ref.exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

cases = [
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "max_max_02", "max_max_03",
    "max_max_04", "max_max_05", "max_max_06", "max_max_07",
    "fft_killer_00", "fft_killer_01",
]

passed = 0
failed = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"[SKIP] {c}: not found")
        continue

    with open(inp, "rb") as f:
        r_ref = subprocess.run([REF], stdin=f, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, timeout=30)
    with open(inp, "rb") as f:
        r_cur = subprocess.run([CUR], stdin=f, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, timeout=30)

    ref_out = r_ref.stdout.replace(b"\r\n", b"\n")
    cur_out = r_cur.stdout.replace(b"\r\n", b"\n")

    if ref_out == cur_out:
        print(f"[PASS] {c}")
        passed += 1
    else:
        print(f"[FAIL] {c}")
        print(f"  ref bytes={len(ref_out)}, cur bytes={len(cur_out)}")
        ref_lines = ref_out.split(b"\n")
        cur_lines = cur_out.split(b"\n")
        print(f"  ref lines={len(ref_lines)}, cur lines={len(cur_lines)}")
        for j in range(min(len(ref_lines), len(cur_lines))):
            if ref_lines[j] != cur_lines[j]:
                print(f"  Line {j+1}: ref len={len(ref_lines[j])}, cur len={len(cur_lines[j])}")
                print(f"    ref: {ref_lines[j][:80]}...")
                print(f"    cur: {cur_lines[j][:80]}...")
                break
        failed += 1

print(f"\n{'='*40}")
print(f"Total: {passed + failed}, Passed: {passed}, Failed: {failed}")
