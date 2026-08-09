"""Generate large test case for div.cpp benchmarking."""
import random
import sys

sys.set_int_max_str_digits(20000000)

BASE = 10000
rng = random.Random(123)
cases = []
for _ in range(3):
    b = rng.randint(BASE ** 4999, BASE ** 5000 - 1)
    q = rng.randint(BASE ** 4999, BASE ** 5000 - 1)
    r = rng.randint(0, b - 1)
    a = q * b + r
    cases.append((a, b))

with open(r"d:\precious_speed\prof_in_large.txt", "w") as f:
    f.write(f"{len(cases)}\n")
    for a, b in cases:
        f.write(f"{a} {b}\n")
print(f"Generated {len(cases)} large cases (b_len=5000, q_len=5000)")
