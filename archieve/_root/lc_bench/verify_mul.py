#!/usr/bin/env python3
"""verify_mul.py - Verify cur_mul.exe output against Python big integer multiplication
   or against a reference EXE (2nd arg), to avoid slow Python bignum on max_max cases."""
import subprocess
import os
import sys
sys.set_int_max_str_digits(0)

EXE = sys.argv[1] if len(sys.argv) > 1 else r"d:\precious_speed\cur_mul.exe"
REF_EXE = sys.argv[2] if len(sys.argv) > 2 else None  # if set, compare EXE vs REF_EXE outputs
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

cases = [
    "example_00", "small_00", "medium_00", "medium_01", "medium_02", "zero_00",
    "large_00", "large_01", "large_02", "large_small_00",
    "max_max_00", "max_max_01", "max_max_02", "max_max_03",
    "max_max_04", "max_max_05", "max_max_06", "max_max_07",
    "fft_killer_00", "fft_killer_01",
]

def run_exe(exe, inp):
    with open(inp, "rb") as f:
        r = subprocess.run([exe], stdin=f, stdout=subprocess.PIPE,
                          stderr=subprocess.DEVNULL, timeout=120)
    return r.stdout.replace(b"\r\n", b"\n")  # normalize Windows CRLF to LF

passed = 0
failed = 0
for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, c + " .in")
    if not os.path.exists(inp):
        print(f"[SKIP] {c}: input not found", flush=True)
        continue

    print(f"[run  ] {c}...", flush=True)
    actual = run_exe(EXE, inp)

    if REF_EXE is not None:
        expected = run_exe(REF_EXE, inp)
    else:
        # Generate expected output with Python
        with open(inp, "r") as f:
            lines = f.read().strip().split("\n")
        t = int(lines[0])
        expected_lines = []
        for i in range(1, t + 1):
            parts = lines[i].strip().split()
            a_str, b_str = parts[0], parts[1]
            a = int(a_str)
            b = int(b_str)
            expected_lines.append(str(a * b))
        expected = ("\n".join(expected_lines) + "\n").encode()

    if actual == expected:
        print(f"[PASS] {c}", flush=True)
        passed += 1
    else:
        print(f"[FAIL] {c}", flush=True)
        act_dec = actual.decode(errors="replace")
        exp_dec = expected.decode(errors="replace")
        act_lines = act_dec.split("\n")
        exp_lines = exp_dec.split("\n")
        print(f"  actual bytes={len(actual)}, expected bytes={len(expected)}")
        print(f"  actual lines={len(act_lines)}, expected lines={len(exp_lines)}")
        for j in range(min(len(act_lines), len(exp_lines))):
            if act_lines[j] != exp_lines[j]:
                print(f"  Line {j+1}: expected len={len(exp_lines[j])}, got len={len(act_lines[j])}")
                print(f"    expected: {exp_lines[j][:80]}...")
                print(f"    got:      {act_lines[j][:80]}...")
                # Find first char difference
                for k in range(min(len(exp_lines[j]), len(act_lines[j]))):
                    if exp_lines[j][k] != act_lines[j][k]:
                        print(f"    First diff at char {k}: expected '{exp_lines[j][k]}' (0x{ord(exp_lines[j][k]):02x}), got '{act_lines[j][k]}' (0x{ord(act_lines[j][k]):02x})")
                        break
                break
        failed += 1

print(f"\n{'='*40}")
print(f"Total: {passed + failed}, Passed: {passed}, Failed: {failed}")
