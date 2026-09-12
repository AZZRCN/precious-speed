"""Minimal repro for b_len=70"""
import subprocess
import sys
from pathlib import Path

sys.set_int_max_str_digits(100000)

CUR_MOD = Path(r"d:\precious_speed\div_dev\cur_mod.exe")
TMP_IN = Path(r"d:\precious_speed\div_dev\lc_test\repro_min.in")

def run_one(a, b):
    with open(TMP_IN, "w") as f:
        f.write(f"1\n{a} {b}\n")
    r = subprocess.run([str(CUR_MOD)], stdin=open(TMP_IN, "rb"),
                       capture_output=True, timeout=30)
    print(f"  stderr: {r.stderr.decode()[:500]}")
    if r.returncode != 0:
        return None, None, f"rc={r.returncode}"
    line = r.stdout.decode().strip()
    parts = line.split()
    if len(parts) >= 2:
        return parts[0], parts[1], None
    return None, None, f"bad output: {line[:100]}"

# b_len=70 (minimal failing)
b_len = 70
b = 10 ** (4 * b_len) - 1
q = 10 ** (4 * (b_len + 1)) - 1
r = 1
a = q * b + r
print(f"b_len={b_len}, |A|={len(str(a))}, |B|={len(str(b))}")
mq, mr, err = run_one(a, b)
if err:
    print(f"  ERROR: {err}")
else:
    print(f"  expected R={r}")
    print(f"  got      R={mr[:40]}")
    print(f"  {'PASS' if mr == str(r) else 'FAIL'}")
