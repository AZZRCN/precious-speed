#!/usr/bin/env python3
"""Local fast verification: generate random cases, run cur_div.exe, compare with Python divmod."""
import subprocess, random, sys
sys.set_int_max_str_digits(0)

EXE = r"d:\precious_speed\tester\cur_div.exe"

def gen_cases(n, max_digits_a, max_digits_b, seed):
    random.seed(seed)
    lines_in = [str(n)]
    lines_out = []
    for _ in range(n):
        da = random.randint(1, max_digits_a)
        db = random.randint(1, max_digits_b)
        a = random.randint(0, 10**da - 1) if da > 1 else random.randint(0, 9)
        b = random.randint(1, 10**db - 1) if db > 1 else random.randint(1, 9)
        lines_in.append(f"{a} {b}")
        q, r = divmod(a, b)
        lines_out.append(f"{q} {r}")
    return "\n".join(lines_in) + "\n", "\n".join(lines_out) + "\n"

def run_local(inp):
    proc = subprocess.run([EXE], input=inp, capture_output=True, text=True, timeout=30)
    return proc.stdout

def compare(out, ref):
    out_lines = out.strip().split("\n")
    ref_lines = ref.strip().split("\n")
    if len(out_lines) != len(ref_lines):
        return False, f"line count: out={len(out_lines)} ref={len(ref_lines)}"
    for i, (o, r) in enumerate(zip(out_lines, ref_lines)):
        if o.strip() != r.strip():
            return False, f"line {i+1}: out={o[:60]}... ref={r[:60]}..."
    return True, "OK"

def main():
    suites = [
        ("tiny",   50,  5,    3),
        ("small",  50,  20,   10),
        ("medium", 30,  200,  100),
        ("large",  10,  2000, 1000),
        ("1to1",   20,  100,  100),
        ("a_lt_b", 20,  50,   200),
    ]
    all_pass = True
    total = 0
    for name, n, da, db in suites:
        inp, ref = gen_cases(n, da, db, 42)
        try:
            out = run_local(inp)
        except subprocess.TimeoutExpired:
            print(f"[FAIL] {name}: TIMEOUT")
            all_pass = False
            continue
        ok, msg = compare(out, ref)
        if ok:
            print(f"[PASS] {name} (n={n})")
            total += n
        else:
            print(f"[FAIL] {name}: {msg}")
            all_pass = False
    print(f"\n=== {'ALL PASS' if all_pass else 'FAILED'}: {total} cases ===")
    sys.exit(0 if all_pass else 1)

if __name__ == "__main__":
    main()
