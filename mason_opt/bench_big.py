#!/usr/bin/env python3
# Focused large-number division benchmark targeting Core2 cache reuse path
# - Tests k being power-of-2 (cache reuse DISABLED) vs non-power-of-2 (ENABLED)
# - Each case: large divisor * N blocks -> triggers absDivNewtonCore2
import random, subprocess, sys, os, time
sys.set_int_max_str_digits(1000000)

def random_digits(n):
    if n == 0: return ""
    s = [str(random.randint(1,9))]
    for _ in range(1, n):
        s.append(str(random.randint(0,9)))
    return "".join(s)

# 4 limbs per digit group (BASE=1e4), so k = dlen/4 limbs
# Power-of-2 k: dlen = 4 * (2^p). E.g., dlen=2048 -> k=512
# Non-power-of-2 k: dlen = 4 * k where k random in (375..1500) excluding 2^p
# We want blocks = len1/len2 to be moderate (e.g., 5-10) so cache reuse path runs

test_groups = {
    # k IS power of 2 -> cache reuse DISABLED (control: should match baseline)
    "pow2_k_dlen2048_blocks8":  [(random_digits(2048 * 8), random_digits(2048)) for _ in range(10)],
    "pow2_k_dlen4096_blocks6":  [(random_digits(4096 * 6), random_digits(4096)) for _ in range(10)],
    "pow2_k_dlen8192_blocks5":  [(random_digits(8192 * 5), random_digits(8192)) for _ in range(5)],

    # k NOT power of 2 -> cache reuse ENABLED (treatment: suspect source of regression)
    "nonpow2_k_dlen1500_blocks8":  [(random_digits(1500 * 8), random_digits(1500)) for _ in range(10)],
    "nonpow2_k_dlen3000_blocks6":  [(random_digits(3000 * 6), random_digits(3000)) for _ in range(10)],
    "nonpow2_k_dlen5000_blocks5":  [(random_digits(5000 * 5), random_digits(5000)) for _ in range(5)],
    "nonpow2_k_dlen2500_blocks10": [(random_digits(2500 * 10), random_digits(2500)) for _ in range(5)],
}

random.seed(12345)  # reset to make data deterministic-ish
# Regenerate with seed for reproducibility
test_groups = {}
random.seed(12345)
test_groups["pow2_k_dlen2048_blocks8"] = [(random_digits(2048 * 8), random_digits(2048)) for _ in range(10)]
test_groups["pow2_k_dlen4096_blocks6"] = [(random_digits(4096 * 6), random_digits(4096)) for _ in range(10)]
test_groups["pow2_k_dlen8192_blocks5"] = [(random_digits(8192 * 5), random_digits(8192)) for _ in range(5)]

test_groups["nonpow2_k_dlen1500_blocks8"]  = [(random_digits(1500 * 8), random_digits(1500)) for _ in range(10)]
test_groups["nonpow2_k_dlen3000_blocks6"]  = [(random_digits(3000 * 6), random_digits(3000)) for _ in range(10)]
test_groups["nonpow2_k_dlen5000_blocks5"]  = [(random_digits(5000 * 5), random_digits(5000)) for _ in range(5)]
test_groups["nonpow2_k_dlen2500_blocks10"] = [(random_digits(2500 * 10), random_digits(2500)) for _ in range(5)]

def write_input(cases, path):
    with open(path, "w", newline="\n") as f:
        f.write(f"{len(cases)}\n")
        for a, b in cases:
            f.write(f"{a} {b}\n")

def run_exe(exe, in_path, out_path):
    with open(in_path, "r") as fin, open(out_path, "w") as fout:
        t0 = time.perf_counter()
        subprocess.run([f"./{exe}"], stdin=fin, stdout=fout, check=True)
        return time.perf_counter() - t0

def verify_output(cases, out_path):
    with open(out_path, "r") as f:
        lines = f.read().split("\n")
    fail = 0
    for i, (a, b) in enumerate(cases):
        if i >= len(lines): break
        line = lines[i].strip()
        if not line: fail += 1; continue
        parts = line.split(" ")
        if len(parts) != 2: fail += 1; continue
        A, B = int(a), int(b)
        try:
            q, r = int(parts[0]), int(parts[1])
        except: fail += 1; continue
        if q * B + r != A or not (0 <= r < B): fail += 1
    return fail

def run_test(name, exe, cases):
    in_path = f"_{name}_in.txt"; out_path = f"_{name}_out.txt"
    write_input(cases, in_path)
    elapsed = run_exe(exe, in_path, out_path)
    fail = verify_output(cases, out_path)
    os.remove(in_path); os.remove(out_path)
    return elapsed, fail

print(f"{'group':<32} {'opt(ms)':>9} {'base(ms)':>10} {'fail':>5} {'speedup':>8}")
print("-" * 70)
for name, cases in test_groups.items():
    opt_t, opt_fail = run_test(f"{name}_opt", "div_opt.exe", cases)
    base_t, base_fail = run_test(f"{name}_base", "div_1st_baseline.exe", cases)
    speedup = base_t / opt_t if opt_t > 0 else 0
    fail = opt_fail + base_fail
    print(f"{name:<32} {opt_t*1000:>9.1f} {base_t*1000:>10.1f} {fail:>5} {speedup:>7.2f}x")
