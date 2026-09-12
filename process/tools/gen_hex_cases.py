#!/usr/bin/env python3
# Generate HEX-faithful division test inputs at EXACT sizes from LC gen/*.cpp +
# info.toml. Digits are random (instruction count depends only on na/nb size,
# not on digit values), so this faithfully reproduces the cost distribution of
# "Division of Hex Big Integers" for offline AMD-instruction analysis.
import os, random

OUT = os.path.join(os.path.dirname(__file__), "cases_hex")
os.makedirs(OUT, exist_ok=True)

SUM = 3200002          # info.toml SUM_OF_CHARACTER_LENGTH
LOG = 1600000          # info.toml LOG_16_A_AND_B_MAX
HEX = "0123456789ABCDEF"

def rhex(n, seed):
    if n <= 0:
        return "0"
    r = random.Random(seed)
    s = "".join(HEX[r.randint(0, 15)] for _ in range(n))
    if n >= 2:
        s = HEX[r.randint(1, 15)] + s[1:]   # non-zero leading digit
    return s

def write(name, pairs):
    with open(os.path.join(OUT, name + ".in"), "w") as f:
        f.write(f"{len(pairs)}\n")
        for a, b in pairs:
            f.write(f"{a} {b}\n")
    tot = sum(len(a) + len(b) for a, b in pairs)
    print(f"  {name:22s} cases={len(pairs):3d}  chars={tot}")

# ---- length_ratio_integer : 6 seeds, m in {2,3,5,10,20,50}, t=2 pairs each ----
print("length_ratio_integer:")
for i, m in enumerate([2, 3, 5, 10, 20, 50]):
    blen = SUM // ((m + 1) * 2)          # gen: SUM/((m+1)*t), t=2
    alen = blen * m
    pairs = [(rhex(alen, 1000 + i * 20 + k), rhex(blen, 2000 + i * 20 + k))
             for k in range(2)]
    write(f"length_ratio_{i}", pairs)

# ---- a_max_b_random : 3 seeds, A=LOG, B uniform(1,LOG-1) ----
print("a_max_b_random:")
for i, frac in enumerate([1, 2, 4]):
    write(f"amax_{i}", [(rhex(LOG, 3000 + i), rhex(LOG // frac, 4000 + i))])

# ---- max : 3 seeds ----
print("max:")
write("max_0", [(rhex(LOG, 5000), rhex(LOG, 5001))])        # A=B=max
write("max_1", [(rhex(LOG, 5002), "1")])                    # A=max / 1
write("max_2", [(rhex(LOG - 1, 5003), rhex(LOG, 5004))])    # A=max-1 / max

# ---- large : 2 seeds, alen,blen uniform(1,80000) ----
print("large:")
for i in range(2):
    pairs, lsum, rr = [], 0, random.Random(6000 + i)
    while True:
        al = rr.randint(1, 80000); bl = rr.randint(1, 80000)
        if rr.randint(0, 9) and al < bl:
            al, bl = bl, al
        lsum += al + bl
        if lsum > SUM:
            break
        j = len(pairs)
        pairs.append((rhex(al, 7000 + i * 100 + j), rhex(bl, 8000 + i * 100 + j)))
    write(f"large_{i}", pairs)

# ---- power : 1 seed, A up to ~533K hex, B=A (na~nb trivial) ----
print("power:")
write("power_0", [(rhex(533000, 9000), rhex(533000, 9000))])

# ---- r_nearly_zero : 3 seeds, upper in {8,800,80000} ----
print("r_nearly_zero:")
for i, upper in enumerate([8, 800, 80000]):
    pairs, lsum, rr = [], 0, random.Random(9100 + i)
    while True:
        al = rr.randint(1, upper); bl = rr.randint(1, upper)
        if al < bl:
            al, bl = bl, al
        lsum += al + bl
        if lsum > SUM:
            break
        j = len(pairs)
        pairs.append((rhex(al, 9200 + i * 100 + j), rhex(bl, 9300 + i * 100 + j)))
    write(f"rnear_{i}", pairs)

# ---- burnikel_ziegler_bound : 4 seeds, na~2nb up to ~80K hex (approx envelope) ----
print("burnikel_ziegler_bound:")
for i in range(4):
    nb = 40000
    write(f"bzbound_{i}", [(rhex(2 * nb, 9400 + i), rhex(nb, 9500 + i))])

# ---- small / medium : tiny, up to 10000 hex ----
print("small/medium:")
for grp in ["small_0", "medium_0", "medium_1", "medium_2"]:
    pairs, lsum, rr = [], 0, random.Random(hash(grp) % 100000)
    while True:
        al = rr.randint(1, 10000); bl = rr.randint(1, 10000)
        if rr.randint(0, 9) and al < bl:
            al, bl = bl, al
        lsum += al + bl
        if lsum > SUM:
            break
        j = len(pairs)
        pairs.append((rhex(al, 9600 + len(grp) * 7 + j), rhex(bl, 9700 + len(grp) * 7 + j)))
    write(grp, pairs)

print("DONE ->", OUT)
