#!/usr/bin/env python3
"""Debug cyclic path: generate large case, compare cyclic vs linear output."""
import subprocess, random, sys
sys.set_int_max_str_digits(0)

EXE = r"d:\precious_speed\tester\cur_div.exe"

def gen_one(max_da, max_db, seed):
    random.seed(seed)
    da = random.randint(1, max_da)
    db = random.randint(1, max_db)
    a = random.randint(0, 10**da - 1) if da > 1 else random.randint(0, 9)
    b = random.randint(1, 10**db - 1) if db > 1 else random.randint(1, 9)
    return a, b

def run_exe(inp):
    proc = subprocess.run([EXE], input=inp, capture_output=True, text=True, timeout=30)
    return proc.stdout, proc.stderr

def main():
    # Generate the same "large" suite cases (seed=42, n=10, da=2000, db=1000)
    random.seed(42)
    n = 10
    lines_in = [str(n)]
    cases = []
    for _ in range(n):
        da = random.randint(1, 2000)
        db = random.randint(1, 1000)
        a = random.randint(0, 10**da - 1) if da > 1 else random.randint(0, 9)
        b = random.randint(1, 10**db - 1) if db > 1 else random.randint(1, 9)
        lines_in.append(f"{a} {b}")
        cases.append((a, b))

    inp = "\n".join(lines_in) + "\n"
    out, err = run_exe(inp)

    out_lines = out.strip().split("\n")
    if len(out_lines) != n:
        print(f"ERROR: output line count {len(out_lines)} != {n}")
        print(f"stderr: {err[:500]}")
        return

    for i, (a, b) in enumerate(cases):
        q, r = divmod(a, b)
        ref = f"{q} {r}"
        got = out_lines[i].strip()
        if got != ref:
            # Find first diff position
            min_len = min(len(got), len(ref))
            diff_pos = -1
            for j in range(min_len):
                if got[j] != ref[j]:
                    diff_pos = j
                    break
            if diff_pos < 0:
                diff_pos = min_len

            # Parse q and r from got
            parts = got.split()
            got_q = parts[0] if len(parts) > 0 else ""
            got_r = parts[1] if len(parts) > 1 else ""

            print(f"Case {i+1} FAIL:")
            print(f"  a digits={len(str(a))}, b digits={len(str(b))}")
            print(f"  ref q digits={len(str(q))}, r digits={len(str(r))}")
            print(f"  got q digits={len(got_q)}, r digits={len(got_r)}")
            print(f"  diff at pos {diff_pos}:")
            print(f"    ref: ...{ref[max(0,diff_pos-10):diff_pos+10]}...")
            print(f"    got: ...{got[max(0,diff_pos-10):diff_pos+10]}...")

            # Check if q or r is wrong
            if got_q == str(q):
                print(f"  Q is CORRECT, R is WRONG")
                print(f"    ref r={r}")
                print(f"    got r={got_r}")
            elif got_r == str(r):
                print(f"  R is CORRECT, Q is WRONG")
            else:
                print(f"  Both Q and R are WRONG")
        else:
            print(f"Case {i+1} PASS (a={len(str(a))}d, b={len(str(b))}d)")

    if err:
        print(f"\nstderr (first 500 chars): {err[:500]}")

if __name__ == "__main__":
    main()
