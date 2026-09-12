"""
Red team: generate precision-strike test cases targeting div_modular.cpp vulnerabilities.

Vulnerability analysis:
1. absDivMu r-based correction: qhat off-by-1, product carry not propagating to tprod[len2]
   - Trigger: divisor * 1 carry doesn't reach high limb
   - Strike: divisor high limb = BASE/2, qhat high limb = 2 (carry at boundary)

2. Normalization boundary: divisor high limb = BASE/2 (minimum normalized)
   - Strike: divisor = (BASE/2) * B^(k-1)

3. Quotient near max: quotient = B^k - 1
   - Strike: A = (B^k - 1) * divisor + (divisor - 1)

4. Remainder near divisor: remainder = divisor - 1
   - Strike: tests final check absCompare(rp, divisor) >= 0

5. Remainder = 0: A = quotient * divisor
   - Strike: tests exact division

6. Block boundary: |A|/|B| ratio aligns with in/len2
   - Strike: |A| = 2*|B|, 3*|B|, 4*|B|

7. divisor = B^k - 1 (max normalized): all limbs = BASE-1
   - Strike: worst case for carry propagation

8. divisor = B^k/2 + 1 (min normalized): high limb = BASE/2
   - Strike: boundary of normalization

9. Large random (FFT precision stress): 10k-100k digits
   - Strike: trigger FFT rounding errors

10. burnikel_ziegler style: remainder nearly zero, quotient = 2*B^(2k)-1
"""
import sys
import random
sys.set_int_max_str_digits(10000000)

BASE = 10**4  # moptm internal base

def b_pow(k):
    return BASE ** k

cases = []

# === Category 1: product carry not propagating (CRITICAL) ===
# divisor high limb = BASE/2, qhat high limb = 2
# divisor * qhat = (BASE/2) * 2 = BASE -> carry exactly at boundary
for k in [2, 4, 8, 16, 32, 64]:
    divisor = (BASE // 2) * b_pow(k-1) + 1  # normalized
    quotient = 2 * b_pow(k) - 1  # qhat high = 2
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Category 2: normalization boundary ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = (BASE // 2) * b_pow(k-1)  # high limb exactly BASE/2
    quotient = b_pow(k) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Category 3: quotient near max ===
for k in [2, 4, 8, 16, 32, 64, 128]:
    divisor = b_pow(k) - 1  # max normalized
    quotient = b_pow(k) - 1  # near max
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Category 4: remainder near divisor ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k*2) - 1
    remainder = divisor - 1  # remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Category 5: exact division (remainder = 0) ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k*2) - 1
    A = quotient * divisor  # remainder = 0
    cases.append((str(A), str(divisor)))

# === Category 6: block boundary (|A|/|B| = 2, 3, 4) ===
for ratio in [2, 3, 4, 5, 10]:
    for k in [4, 8, 16, 32]:
        divisor = b_pow(k) - 1
        quotient = b_pow(k * ratio) - 1
        remainder = divisor // 2
        A = quotient * divisor + remainder
        cases.append((str(A), str(divisor)))

# === Category 7: divisor = B^k - 1 (all limbs BASE-1) ===
for k in [2, 4, 8, 16, 32, 64, 128, 256]:
    divisor = b_pow(k) - 1
    quotient = b_pow(k + 1) - 1
    remainder = 0
    A = quotient * divisor
    cases.append((str(A), str(divisor)))

# === Category 8: divisor = B^k/2 + 1 (min normalized) ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) // 2 + 1
    quotient = b_pow(k + 1) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# === Category 9: large random (FFT precision stress) ===
rng = random.Random(42)
for _ in range(30):
    # digits in decimal
    alen = rng.randint(1000, 50000)
    blen = rng.randint(1000, 50000)
    if alen < blen:
        alen, blen = blen, alen
    A = rng.randint(10**(alen-1), 10**alen - 1)
    B = rng.randint(10**(blen-1), 10**blen - 1)
    cases.append((str(A), str(B)))

# === Category 10: burnikel_ziegler style ===
# quotient = 2*B^(2k) - 1, remainder = (B^k - 1) * B^k, y = remainder + 1
for k in [2, 4, 8, 16, 32]:
    bb = b_pow(k)
    quotient = 2 * bb * bb - 1
    remainder = (bb - 1) * bb
    y = remainder + 1
    x = quotient * y + remainder
    cases.append((str(x), str(y)))

# === Category 11: A = divisor (quotient = 1) ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    A = divisor
    cases.append((str(A), str(divisor)))

# === Category 12: A = divisor - 1 (quotient = 0) ===
for k in [2, 4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    A = divisor - 1
    cases.append((str(A), str(divisor)))

# === Category 13: divisor = 1 ===
cases.append((str(b_pow(100)), "1"))
cases.append((str(b_pow(1000)), "1"))

# === Category 14: A = B^k (power of base) ===
for k in [4, 8, 16, 32, 64]:
    divisor = b_pow(k) - 1
    A = b_pow(k * 2)
    cases.append((str(A), str(divisor)))

# === Category 15: adversarial carry patterns ===
# divisor with alternating 0 and BASE-1 limbs
for k in [4, 8, 16, 32]:
    divisor = 0
    for i in range(k):
        if i % 2 == 0:
            divisor += (BASE - 1) * b_pow(i)
        else:
            divisor += 0  # zero limb
    if divisor < b_pow(k) // 2:  # ensure normalized
        divisor += (BASE // 2) * b_pow(k-1)
    quotient = b_pow(k + 1) - 1
    remainder = divisor - 1
    A = quotient * divisor + remainder
    cases.append((str(A), str(divisor)))

# Write output
out_path = r"d:\precious_speed\div_dev\lc_test\red_team.in"
with open(out_path, "w") as f:
    f.write(f"{len(cases)}\n")
    for a, b in cases:
        f.write(f"{a} {b}\n")

print(f"Generated {len(cases)} precision-strike test cases -> {out_path}")

# Print summary
from collections import Counter
print(f"Categories: {len(cases)} cases total")
