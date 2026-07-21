#!/usr/bin/env python3
# DIV boundary tests for scheme D (blocks >= 2 fast path)
# Tests blocks=2 (exact + remainder), blocks=3+, blocks=1 (Core1), small, negatives
import subprocess
import random
import sys

sys.set_int_max_str_digits(0)  # no limit for big-integer string conversion

WORK = "/tmp/o2compare"
BIN = f"{WORK}/mf_DIV"

def rand_digits(n, seed=None):
    """Generate a random integer string of exactly n decimal digits (no leading zero)."""
    if seed is not None:
        random.seed(seed)
    if n <= 0:
        return "0"
    s = str(random.randint(1, 9))
    for _ in range(n - 1):
        s += str(random.randint(0, 9))
    return s

def run_div(pairs):
    """Run mf_DIV on a list of (a_str, b_str) pairs. Returns list of 'q r' strings."""
    n = len(pairs)
    inp = f"{n}\n" + "\n".join(f"{a} {b}" for a, b in pairs) + "\n"
    p = subprocess.run([BIN], input=inp, capture_output=True, text=True, timeout=300)
    if p.returncode != 0:
        print(f"[ERROR] mf_DIV exited rc={p.returncode}")
        print(f"[STDERR] {p.stderr[:500]}")
        sys.exit(2)
    lines = p.stdout.rstrip("\n").split("\n")
    if len(lines) != n:
        print(f"[ERROR] expected {n} output lines, got {len(lines)}")
        sys.exit(2)
    return lines

def expected_div(a_str, b_str):
    """Python reference: absDivRem semantics — |a| / |b|, q and r non-negative, r < |b|."""
    a = int(a_str); b = int(b_str)
    a = abs(a); b = abs(b)
    if b == 0:
        return "ERROR_DIV0"
    q, r = divmod(a, b)
    return f"{q} {r}"

def check(label, pairs):
    got = run_div(pairs)
    fails = 0
    for i, (pair, g) in enumerate(zip(pairs, got)):
        exp = expected_div(pair[0], pair[1])
        if exp != g:
            fails += 1
            if fails <= 3:
                print(f"  [{label}] case {i} FAIL: a_digits={len(pair[0])} b_digits={len(pair[1])}")
                print(f"    exp={exp[:100]}...")
                print(f"    got={g[:100]}...")
    status = "OK" if fails == 0 else f"FAIL({fails}/{len(pairs)})"
    print(f"  [{label}] {status} ({len(pairs)} cases)")
    return fails == 0

def main():
    random.seed(42)
    all_ok = True
    print("=== DIV Boundary Tests (scheme D: blocks >= 2 fast path) ===")

    # 1. blocks=2 exact boundary (len1 = 2*len2 in limbs, no remainder)
    #    These are the NEW cases enabled by the change.
    pairs = []
    for la, lb in [(900000, 450000), (450000, 225000), (180000, 90000), (90000, 45000)]:
        a = rand_digits(la, seed=la)
        b = rand_digits(lb, seed=lb)
        pairs.append((a, b))
    all_ok &= check("blocks=2 exact (no rem)", pairs)

    # 2. blocks=2 with remainder (len1 = 2*len2 + r, 0 < r < len2)
    pairs = []
    for la, lb in [(910000, 450000), (460000, 225000), (185000, 90000), (95000, 45000)]:
        a = rand_digits(la, seed=la)
        b = rand_digits(lb, seed=lb)
        pairs.append((a, b))
    all_ok &= check("blocks=2 with remainder", pairs)

    # 3. blocks=3 (len1 >= 3*len2) — existing fast path, should still work
    pairs = []
    for la, lb in [(1350000, 450000), (1000000, 333000), (270000, 90000), (135000, 45000)]:
        a = rand_digits(la, seed=la)
        b = rand_digits(lb, seed=lb)
        pairs.append((a, b))
    all_ok &= check("blocks=3+ (existing fast)", pairs)

    # 4. blocks=4+ (1M/250k) — also existing fast path
    pairs = []
    for la, lb in [(1000000, 250000), (400000, 90000)]:
        a = rand_digits(la, seed=la)
        b = rand_digits(lb, seed=lb)
        pairs.append((a, b))
    all_ok &= check("blocks=4+", pairs)

    # 5. blocks=1 (len1 < 2*len2) — Core1, not affected by change
    pairs = []
    for la, lb in [(500000, 500000), (500000, 490000), (460000, 450000), (45100, 45000)]:
        a = rand_digits(la, seed=la)
        b = rand_digits(lb, seed=lb)
        pairs.append((a, b))
    all_ok &= check("blocks=1 (Core1)", pairs)

    # 6. Boundary near 64 limbs (threshold for Core2 vs BasicCore)
    #    64 limbs ≈ 576 digits. Test around 60-70 limbs.
    pairs = []
    for la, lb in [(1300, 600), (1200, 600), (1300, 650), (1260, 600)]:
        a = rand_digits(la, seed=la)
        b = rand_digits(lb, seed=lb)
        pairs.append((a, b))
    all_ok &= check("near 64-limb threshold", pairs)

    # 7. Small inputs
    pairs = []
    small_cases = [
        ("100", "50"), ("10", "5"), ("20", "10"), ("99", "3"), ("100", "1"),
        ("1000000", "1"), ("123456789", "987654321"),  # a < b
        ("12345678901234567890", "123"), ("100000000000000000000", "99999999999"),
        ("999999999999999999", "999999999999999999"),  # equal
        ("1000000000", "1000000000"),  # equal, power of 10
    ]
    pairs.extend(small_cases)
    all_ok &= check("small inputs", pairs)

    # 8. Negative tests (absDivRem takes absolute values)
    pairs = []
    neg_cases = [
        ("-100", "50"), ("100", "-50"), ("-100", "-50"),
        ("-1000000", "500000"), ("-999999999", "333333333"),
        ("-123456789012345678901234567890", "1234567890"),
    ]
    pairs.extend(neg_cases)
    all_ok &= check("negative inputs", pairs)

    # 9. Edge: b=1 (q=a, r=0)
    pairs = []
    for n in [1, 5, 100, 1000, 10000, 100000]:
        a = rand_digits(n, seed=n)
        pairs.append((a, "1"))
    all_ok &= check("b=1 (identity)", pairs)

    # 10. Stress: many blocks=2 cases with different random seeds
    pairs = []
    for seed in range(20):
        a = rand_digits(90000, seed=seed*1000+1)
        b = rand_digits(45000, seed=seed*1000+2)
        pairs.append((a, b))
    all_ok &= check("stress blocks=2 (20 random)", pairs)

    print("=" * 50)
    print("ALL_PASS" if all_ok else "SOME_FAIL")
    sys.exit(0 if all_ok else 1)

if __name__ == "__main__":
    main()
