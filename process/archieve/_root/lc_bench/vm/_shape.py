import glob
import os

d = "/home/azzr/lcp/big_integer/division_of_big_integers/in/"
pats = ["length_ratio_integer_*.in", "a_max_b_random_*.in",
        "burnikel_ziegler_bound_*.in", "large_*.in", "max_*.in", "medium_*.in",
        "r_nearly_zero_*.in"]
for p in pats:
    for f in sorted(glob.glob(d + p)):
        L = open(f).read().split("\n")
        T = int(L[0])
        rows = []
        for i in range(1, min(T, 4) + 1):
            a, b = L[i].split()
            rows.append((len(a), len(b), round(len(a) / len(b), 3)))
        print("%-32s T=%-5d %s" % (os.path.basename(f), T, rows))
