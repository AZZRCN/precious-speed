"""Generate profile test input for div.cpp profiling."""
import random
import sys

sys.set_int_max_str_digits(2000000)

BASE = 10000
rng = random.Random(42)
cases = []
for _ in range(3):
    b = rng.randint(BASE ** 999, BASE ** 1000 - 1)
    # Make a = q*b + r with q having ~1000 limbs
    q = rng.randint(BASE ** 999, BASE ** 1000 - 1)
    r = rng.randint(0, b - 1)
    a = q * b + r
    cases.append((a, b))

with open(r"d:\precious_speed\prof_in.txt", "w") as f:
    f.write(f"{len(cases)}\n")
    for a, b in cases:
        f.write(f"{a} {b}\n")
print(f"Generated {len(cases)} cases")
