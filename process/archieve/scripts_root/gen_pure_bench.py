"""Generate test cases for pure div bench."""
import random
import sys

sys.set_int_max_str_digits(2000000)

BASE = 10000

cases = []
for b_len, q_len, n in [(500, 500, 10), (1000, 1000, 5), (5000, 5000, 3), (10000, 10000, 2), (50000, 50000, 1)]:
    for i in range(n):
        rng = random.Random(b_len * 10000 + q_len * 100 + i)
        b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
        q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
        r = rng.randint(0, b - 1)
        a = q * b + r
        cases.append((a, b))

with open(r"d:\precious_speed\pure_bench.in", "w") as f:
    f.write(f"{len(cases)}\n")
    for a, b in cases:
        f.write(f"{a} {b}\n")

print(f"Generated {len(cases)} cases")
