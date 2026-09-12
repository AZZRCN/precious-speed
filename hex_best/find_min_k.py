B = 65536
HALF = B//2

def min_k_bruteforce():
    # For HEX: high is 32-bit (high1*B+high2, high1,high2 < B => high < B^2 = 2^32).
    # divisor_high d in [HALF, B).
    # GM: qh = floor( (floor(2^K/d)+1) * high / 2^K ). Want qh == high//d for all valid (high,d).
    # Sample high and d densely.
    import random
    random.seed(7)
    # build sample of d (16-bit, normalized) and high (32-bit)
    ds = [random.randint(HALF, B-1) for _ in range(400)]
    # add edge ds
    ds += [HALF, HALF+1, B-2, B-1, 40000, 60071]
    highs = [random.randint(0, B*B-1) for _ in range(400)]
    # edge highs
    highs += [0, 1, B-1, B, B*B-1, B*B-2, (B//2)*B + (B//2)]
    results = {}
    for K in range(48, 65):
        fails = 0
        for d in ds:
            m = ((1 << K) // d) + 1
            for high in highs:
                if high1 := (high // B) >= d:
                    # branch B-1; skip (handled separately, exact)
                    continue
                qh = (m * high) >> K
                if qh != high // d:
                    fails += 1
                    if fails <= 2:
                        pass
        results[K] = fails
    return results

res = min_k_bruteforce()
for K in sorted(res):
    print(f"K={K}: failures={res[K]}")
