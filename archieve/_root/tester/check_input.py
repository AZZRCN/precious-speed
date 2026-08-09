import sys

with open(r"d:\precious_speed\tester\failures\div_div_medium_98_0.in", "r") as f:
    lines = [f.readline().strip() for _ in range(5)]
    for i, l in enumerate(lines):
        print(f"line {i}: [{l[:80]}...] len={len(l)}")