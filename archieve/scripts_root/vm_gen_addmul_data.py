#!/usr/bin/env python3
"""Generate ADD/MUL test data of various scales."""
import random
import sys
import os

sys.set_int_max_str_digits(20000000)

BASE = 10000

def gen_add_case(len_a, len_b, rng):
    """Generate a+b test case."""
    a = rng.randint(BASE ** (len_a - 1), BASE ** len_a - 1)
    b = rng.randint(BASE ** (len_b - 1), BASE ** len_b - 1)
    return a, b

def gen_mul_case(len_a, len_b, rng):
    """Generate a*b test case."""
    a = rng.randint(BASE ** (len_a - 1), BASE ** len_a - 1)
    b = rng.randint(BASE ** (len_b - 1), BASE ** len_b - 1)
    return a, b

def gen_file(filename, cases, op='add', seed=42):
    rng = random.Random(seed)
    with open(filename, 'w') as f:
        f.write(f"{len(cases)}\n")
        for la, lb in cases:
            if op == 'add':
                a, b = gen_add_case(la, lb, rng)
            else:
                a, b = gen_mul_case(la, lb, rng)
            f.write(f"{a} {b}\n")
    sz = os.path.getsize(filename)
    print(f"  {filename}: {len(cases)} cases, {sz/1024:.1f} KB")

if __name__ == '__main__':
    print("Generating ADD test data...")
    # small: 70 cases, b=50~100 limbs (200~400 digits)
    small = [(random.Random(i).randint(50, 100), random.Random(i+1000).randint(50, 100)) for i in range(70)]
    gen_file("add_small_0.in", small, 'add', seed=100)

    # medium: ~1000 cases, b=1~10 limbs (4~40 digits)
    medium = [(random.Random(i).randint(1, 10), random.Random(i+2000).randint(1, 10)) for i in range(1000)]
    gen_file("add_medium_0.in", medium, 'add', seed=200)

    # large: 5 cases, ~100k limbs (400k digits)
    large = [(100000, 100000), (50000, 50000), (80000, 80000), (100000, 50000), (50000, 100000)]
    gen_file("add_large_0.in", large, 'add', seed=300)

    # xlarge: 1 case, 500k limbs (2M digits)
    gen_file("add_xlarge_0.in", [(500000, 500000)], 'add', seed=400)

    # xxlarge: 1 case, 1M limbs (4M digits) - too large, skip
    # gen_file("add_xxlarge_0.in", [(1000000, 1000000)], 'add', seed=500)

    print("\nGenerating MUL test data...")
    # small: 70 cases, b=50~100 limbs
    gen_file("mul_small_0.in", small, 'mul', seed=1100)

    # medium: ~1000 cases, b=1~10 limbs
    gen_file("mul_medium_0.in", medium, 'mul', seed=1200)

    # large: 5 cases, ~10k limbs (40k digits each)
    large_mul = [(10000, 10000), (5000, 5000), (8000, 8000), (10000, 5000), (5000, 10000)]
    gen_file("mul_large_0.in", large_mul, 'mul', seed=1300)

    # xlarge: 1 case, 50k limbs (200k digits)
    gen_file("mul_xlarge_0.in", [(50000, 50000)], 'mul', seed=1400)

    # xxlarge: 1 case, 100k limbs (400k digits)
    gen_file("mul_xxlarge_0.in", [(100000, 100000)], 'mul', seed=1500)

    print("\nDone.")
