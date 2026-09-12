#!/usr/bin/env python3
"""Verify cyclic path with huge numbers (50000+ digits)."""
import subprocess, random, sys
sys.set_int_max_str_digits(0)

EXE = r"d:\precious_speed\tester\cur_div.exe"

def run_exe(inp):
    proc = subprocess.run([EXE], input=inp, capture_output=True, text=True, timeout=60)
    return proc.stdout, proc.stderr

def main():
    random.seed(42)
    # Huge cases: trigger absInvNewton cyclic path (k >= 4096 limbs = 16384 digits)
    suites = [
        ("huge_50k_20k", 5, 50000, 20000),   # 5 cases, a<=50k digits, b<=20k digits
        ("huge_100k_50k", 3, 100000, 50000),  # 3 cases, a<=100k digits, b<=50k digits
        ("huge_30k_30k",  5, 30000, 30000),   # a ~= b
    ]

    all_pass = True
    total = 0
    for name, n, da, db in suites:
        print(f"=== {name} (n={n}, da<={da}, db<={db}) ===", flush=True)
        lines_in = [str(n)]
        cases = []
        for _ in range(n):
            da_i = random.randint(da//2, da)
            db_i = random.randint(db//2, db)
            a = random.randint(0, 10**da_i - 1)
            b = random.randint(1, 10**db_i - 1)
            lines_in.append(f"{a} {b}")
            cases.append((a, b))

        inp = "\n".join(lines_in) + "\n"
        try:
            out, err = run_exe(inp)
        except subprocess.TimeoutExpired:
            print(f"  [FAIL] TIMEOUT")
            all_pass = False
            continue

        out_lines = out.strip().split("\n")
        if len(out_lines) != n:
            print(f"  [FAIL] output line count {len(out_lines)} != {n}")
            print(f"  stderr: {err[:300]}")
            all_pass = False
            continue

        for i, (a, b) in enumerate(cases):
            q, r = divmod(a, b)
            ref = f"{q} {r}"
            got = out_lines[i].strip()
            if got != ref:
                print(f"  [FAIL] case {i+1}: a={len(str(a))}d b={len(str(b))}d")
                # Find diff
                min_len = min(len(got), len(ref))
                for j in range(min_len):
                    if got[j] != ref[j]:
                        print(f"    diff at pos {j}: ref=...{ref[max(0,j-5):j+5]}... got=...{got[max(0,j-5):j+5]}...")
                        break
                all_pass = False
            else:
                print(f"  [PASS] case {i+1}: a={len(str(a))}d b={len(str(b))}d")
                total += 1

    print(f"\n=== {'ALL PASS' if all_pass else 'FAILED'}: {total} cases ===")
    sys.exit(0 if all_pass else 1)

if __name__ == "__main__":
    main()
