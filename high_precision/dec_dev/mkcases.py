import random

def rh(nlimbs, rng):
    n = nlimbs * 4
    s = "".join(rng.choice("0123456789abcdef") for _ in range(n))
    s = rng.choice("123456789abcdef") + s[1:]
    return s

def force_ge(a, na, b, nb):
    if na > nb:
        return a
    if a >= b:
        return a
    lst = int(a[:16], 16)
    blst = int(b[:16], 16)
    if lst < blst:
        nlst = blst | 0x8000000000000000
        if nlst < blst:
            nlst = blst + 1
        a = format(nlst, "016x") + a[16:]
    return a

def gen(specs, fname, seed=20260816):
    rng = random.Random(seed)
    out = []
    for (na, nb) in specs:
        if na > 100010 or nb > 100010 or na < 1 or nb < 1:
            continue
        if na - nb + 1 <= 128 and na > nb:
            pass  # still valid (newton if >128; knuthD if <=128) - keep for coverage
        a = rh(na, rng)
        b = rh(nb, rng)
        a = force_ge(a, na, b, nb)
        out.append((na, nb, a, b))
    with open(fname, "w") as f:
        f.write(str(len(out)) + "\n")
        for na, nb, a, b in out:
            f.write(a + "\n" + b + "\n")
    print(fname, "cases=", len(out))

if __name__ == "__main__":
    # NEWTON-heavy: na-nb+1 in (129, nb], large nb (invertappr(n=nb) dominant)
    specs = []
    for nb in [99000, 98304, 95000, 90000, 80000, 70000, 60000, 50000, 40000, 30000, 20000, 10000]:
        specs.append((100000, nb))          # na=100000 fixed, nb varies -> newton (q from 1001..90001)
    specs.append((100010, 99882))            # max nb, q=129
    specs.append((99000, 98000))             # q=1001
    specs.append((90000, 89000))
    specs.append((80000, 79000))
    # na=2nb newton (q ~ nb limbs)
    specs.append((100000, 50000))
    specs.append((90000, 45000))
    # knuthD reference (q<=128)
    specs.append((100000, 99996))            # q=5 knuthD
    specs.append((100000, 99872))            # q=129 newton edge
    gen(specs, "bigcases.in")
    gen([(100000, 99000)], "worst.in")
