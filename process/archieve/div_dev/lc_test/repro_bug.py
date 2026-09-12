"""Minimal reproduction of bug #403: b=10^400-1, q=10^404-1, r=1"""
import subprocess
import sys
from pathlib import Path

sys.set_int_max_str_digits(100000)

CUR_MOD = Path(r"d:\precious_speed\div_dev\cur_mod.exe")
TMP_IN = Path(r"d:\precious_speed\div_dev\lc_test\repro.in")
TMP_OUT = Path(r"d:\precious_speed\div_dev\lc_test\repro.out")

BASE = 10000


def run_one(a, b):
    with open(TMP_IN, "w") as f:
        f.write(f"1\n{a} {b}\n")
    r = subprocess.run([str(CUR_MOD)], stdin=open(TMP_IN, "rb"),
                       capture_output=True, timeout=30)
    if r.returncode != 0:
        return None, None, f"rc={r.returncode}"
    line = r.stdout.decode().strip()
    parts = line.split()
    if len(parts) >= 2:
        return parts[0], parts[1], None
    return None, None, f"bad output: {line[:100]}"


def main():
    # Original failing case
    print("=== Original failing case (b_len=100) ===")
    b_len = 100
    b = (BASE - 1) * (BASE ** (b_len - 1)) + (BASE ** (b_len - 1)) - 1
    # b = 10^400 - 1 (all 9s)
    assert b == 10 ** (4 * b_len) - 1, f"b != 10^400-1, b={b}"
    q = (BASE ** (b_len + 1)) - 1
    r = 1
    a = q * b + r
    eq_q, eq_r = str(q), str(r)
    mq, mr, err = run_one(a, b)
    if err:
        print(f"  ERROR: {err}")
    else:
        print(f"  |A|={len(str(a))} |B|={len(str(b))}")
        print(f"  expected Q={eq_q[:30]}... (len={len(eq_q)})")
        print(f"  got      Q={mq[:30]}... (len={len(mq)})")
        print(f"  expected R={eq_r}")
        print(f"  got      R={mr}")
        if mq == eq_q and mr == eq_r:
            print("  PASS")
        else:
            print("  FAIL")
            # diff position
            for i in range(min(len(mq), len(eq_q))):
                if i < len(mq) and i < len(eq_q) and mq[i] != eq_q[i]:
                    print(f"  Q first diff at position {i}")
                    break

    # Binary search for minimal b_len that fails
    print("\n=== Binary search for minimal failing b_len ===")
    for test_b_len in [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]:
        b = 10 ** (4 * test_b_len) - 1
        q = 10 ** (4 * (test_b_len + 1)) - 1
        r = 1
        a = q * b + r
        mq, mr, err = run_one(a, b)
        if err:
            print(f"  b_len={test_b_len}: ERROR {err}")
        elif mq == str(q) and mr == str(r):
            print(f"  b_len={test_b_len}: PASS")
        else:
            print(f"  b_len={test_b_len}: FAIL (Q diff, R={mr[:30]})")

    # Test with r=1 but different q patterns
    print("\n=== b_len=100, r=1, different q ===")
    b = 10 ** 400 - 1
    for q_desc, q_val in [("10^404-1 (all 9s)", 10**404 - 1),
                           ("10^404-2", 10**404 - 2),
                           ("10^403", 10**403),
                           ("10^403+7", 10**403 + 7)]:
        a = q_val * b + 1
        mq, mr, err = run_one(a, b)
        if err:
            print(f"  q={q_desc}: ERROR {err}")
        elif mq == str(q_val) and mr == "1":
            print(f"  q={q_desc}: PASS")
        else:
            print(f"  q={q_desc}: FAIL R={mr[:30]}")

    # Test b_len=100 with different r
    print("\n=== b_len=100, q=10^404-1, different r ===")
    b = 10 ** 400 - 1
    q = 10 ** 404 - 1
    for r_val in [0, 1, 2, 7, b // 2, b - 1]:
        a = q * b + r_val
        mq, mr, err = run_one(a, b)
        if err:
            print(f"  r={r_val}: ERROR {err}")
        elif mq == str(q) and mr == str(r_val):
            print(f"  r={r_val}: PASS")
        else:
            print(f"  r={r_val}: FAIL got_r={mr[:30]}")


if __name__ == "__main__":
    main()
