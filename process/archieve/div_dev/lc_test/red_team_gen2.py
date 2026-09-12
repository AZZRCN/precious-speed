"""
Red team round 2: precision-strike test cases targeting deeper vulnerabilities.

Focus areas:
1. cyclic unwrap boundary: cyclic_m == wnd_full_len (product high 1 limb wraps)
2. Block boundary carry: qhat overflow at block end (B^this_in - 1 + 1)
3. Multi-block carry propagation: consecutive blocks overflow
4. divisor with specific high limb patterns (BASE/2, BASE/2+1, BASE-1)
5. Quotient = B^k - 1 (all limbs BASE-1, max carry propagation)
6. Tiny divisor (len2=1, but goes through basic path)
7. |A| just slightly > 2*|B| (Core1/Core2 boundary)
8. |A| = 2*|B| + 1 (minimal Core1 case)
9. divisor = B^k + 1 (non-normalized, needs factor)
10. Adversarial FFT precision: numbers with many 9s
11. Cyclic path trigger: specific sizes to force use_cyclic=true
12. Large est_blocks (force cyclic_m = int_ceil2(len2+in+1))
"""
import sys
import random
sys.set_int_max_str_digits(10000000)

BASE = 10**4

def b_pow(k):
    return BASE ** k

cases = []

# === Cat 1: quotient = B^k - 1 (max carry propagation) ===
# Forces qhat = B^this_in - 1, final check +1 causes overflow
for k in [2, 4, 8, 16, 32, 64, 128]:
    divisor = b_pow(k) - 1  # max normalized
    quotient = b_pow(k) - 1  # all limbs BASE-1
    remainder = 0  # exact division, tests carry chain
    A = quotient * divisor
    cases.append((str(A), str(divisor)))

# === Cat 2: quotient = B^k - 1 with remainder = divisor - 1 ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 3: quotient = B^k (power of base, single 1 limb) ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k)
    remainder = 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 4: divisor = B^k/2 (min normalized, boundary) ===
for k in [2, 4, 8, 16, 32, 64, 128]:
    divisor = b_pow(k) // 2
    quotient = b_pow(k + 1) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 5: divisor = B^k/2 + 1 (just above min normalized) ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) // 2 + 1
    quotient = b_pow(k + 1) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 6: consecutive qhat overflow (multi-block carry) ===
# Each block quotient = B^in - 1, final check causes +1 overflow
for in_size in [4, 8, 16, 32]:
    for ratio in [3, 5, 10]:  # multiple blocks
        k = in_size
        divisor = b_pow(k) - 1
        quotient = b_pow(k * ratio) - 1  # all limbs BASE-1
        remainder = 0
        A = quotient * divisor
        cases.append((str(A), str(divisor)))

# === Cat 7: |A| = 2*|B| exactly (Core1 boundary) ===
for k in [4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k) + 1  # |A| = 2*|B| + 1
    remainder = divisor // 2
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 8: |A| = 2*|B| - 1 (just below Core1 boundary) ===
for k in [4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 9: large est_blocks (force cyclic_m = int_ceil2(len2+in+1)) ===
# ratio = 20+ to trigger est_blocks > 10
for ratio in [20, 50, 100]:
    for k in [4, 8, 16]:
        divisor = b_pow(k) - 1
        quotient = b_pow(k * ratio) - 1
        remainder = divisor - 1
        A = quotient * divisor + remainder
        cases.append((str(A), str(divisor)))

# === Cat 10: adversarial FFT precision (many 9s) ===
for digits in [100, 1000, 10000, 50000]:
    A = int("9" * digits)
    B = int("9" * (digits // 2))
    cases.append((str(A), str(B)))

# === Cat 11: divisor = B^k + 1 (needs normalization factor) ===
for k in [4, 8, 16, 32, 64]:
    divisor = b_pow(k) + 1
    quotient = b_pow(k * 2) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 12: divisor = 2 (small, needs normalization) ===
for exp in [10, 100, 1000, 10000]:
    A = b_pow(exp) - 1
    cases.append((str(A), "2"))

# === Cat 13: divisor = B^k - B^(k-1) (high limb = BASE-1, second high = 0) ===
for k in [4, 8, 16, 32, 64]:
    divisor = (BASE - 1) * b_pow(k - 1)
    quotient = b_pow(k + 1) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 14: product carry exact boundary ===
# divisor_high = BASE/2, qhat_high = 2: product_high = BASE (exact carry)
for k in [4, 8, 16, 32, 64]:
    divisor = (BASE // 2) * b_pow(k - 1) + (BASE // 2)
    quotient = 2 * b_pow(k) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Cat 15: random large with specific sizes ===
rng = random.Random(12345)
for _ in range(40):
    # Force specific size ratios
    ratio_choice = rng.choice([2, 3, 5, 10, 20, 50, 100])
    blen = rng.choice([4, 8, 16, 32, 64, 128, 256])
    alen = blen * ratio_choice
    B = rng.randint(b_pow(blen - 1), b_pow(blen) - 1)
    Q = rng.randint(b_pow(alen - blen - 1), b_pow(alen - blen) - 1)
    R = rng.randint(0, B - 1)
    A = Q * B + R
    cases.append((str(A), str(B)))

# === Cat 16: A = B (quotient = 1, trivial) ===
for k in [4, 8, 16, 32, 64, 128, 256]:
    divisor = b_pow(k) - 1
    A = divisor
    cases.append((str(A), str(divisor)))

# === Cat 17: A = B * 2 (quotient = 2) ===
for k in [4, 8, 16, 32, 64, 128]:
    divisor = b_pow(k) - 1
    A = divisor * 2
    cases.append((str(A), str(divisor)))

# === Cat 18: cyclic path specific sizes ===
# in = len2, est_blocks <= 10 to trigger cyclic unwrap
for k in [8, 16, 32]:
    for ratio in [2, 3, 4, 5]:
        divisor = b_pow(k) - 1
        quotient = b_pow(k * ratio) - 1
        remainder = divisor // 3
        A = quotient * divisor + remainder
        cases.append((str(A), str(divisor)))

out_path = r"d:\precious_speed\div_dev\lc_test\red_team2.in"
with open(out_path, "w") as f:
    f.write(f"{len(cases)}\n")
    for a, b in cases:
        f.write(f"{a} {b}\n")

print(f"Generated {len(cases)} precision-strike test cases -> {out_path}")
