#!/usr/bin/env python3
"""Analyze div_medium_0.in: count cases by digit length."""
import os, sys
sys.set_int_max_str_digits(5000000)

os.chdir(os.path.expanduser("~/div_bench"))
with open("div_medium_0.in") as f:
    t = int(f.readline())
    len_dist = {}
    len2_dist = {}
    for _ in range(t):
        a, b = f.readline().split()
        la, lb = len(a), len(b)
        len_dist[la] = len_dist.get(la, 0) + 1
        len2_dist[lb] = len2_dist.get(lb, 0) + 1

print(f"Total cases: {t}")
print(f"\nb digit length distribution (len2 in BASE=10^4):")
for lb in sorted(len2_dist.keys()):
    cnt = len2_dist[lb]
    # BASE=10^4, so len2_limbs = ceil(lb/4)
    len2_limbs = (lb + 3) // 4
    print(f"  {lb:3d} digits ({len2_limbs} limbs): {cnt:6d} cases")

print(f"\na digit length distribution:")
for la in sorted(len_dist.keys()):
    cnt = len_dist[la]
    print(f"  {la:3d} digits: {cnt:6d} cases")
