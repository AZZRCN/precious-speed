#!/usr/bin/env python3
"""Generate test data of various scales for div benchmark."""
import random
import sys

sys.set_int_max_str_digits(20000000)

BASE = 10000

def gen_case(b_len, q_len, rng):
    """Generate one case: a = q*b + r, with b having b_len limbs, q having q_len limbs."""
    b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
    q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1)
    r = rng.randint(0, b - 1)
    a = q * b + r
    return a, b

def gen_file(filename, cases, seed=42):
    rng = random.Random(seed)
    with open(filename, 'w') as f:
        f.write(f"{len(cases)}\n")
        for b_len, q_len in cases:
            a, b = gen_case(b_len, q_len, rng)
            f.write(f"{a} {b}\n")
    import os
    sz = os.path.getsize(filename)
    print(f"  {filename}: {len(cases)} cases, {sz/1024:.1f} KB")

if __name__ == '__main__':
    print("Generating test data...")

    # small: b=50~100 limbs (200~400 digits), 70 cases
    small_cases = [(random.Random(i).randint(50, 100), random.Random(i+1000).randint(50, 100)) for i in range(70)]
    gen_file("div_small_0.in", small_cases, seed=100)

    # xlarge: b=50000 limbs (200k digits), 1 case
    gen_file("div_xlarge_0.in", [(50000, 50000)], seed=50000)

    # xxlarge: b=100000 limbs (400k digits), 1 case
    gen_file("div_xxlarge_0.in", [(100000, 100000)], seed=100000)

    print("Done.")
