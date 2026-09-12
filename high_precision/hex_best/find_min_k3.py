import random
B = 65536
HALF = B//2

def gm_qh(K, d, high):
    m = ((1 << K) // d) + 1
    return (m * high) >> K

def search():
    random.seed(2026)
    bad = (2238379095, 60071)  # known real failure (high1=34154 < 60071)
    pairs = [bad]
    for _ in range(40000):
        d = random.randint(HALF, B-1)
        high1 = random.randint(0, d-1)          # GM branch only
        high2 = random.randint(0, B-1)
        high = high1*B + high2
        pairs.append((high, d))
    # edge: high1 just below d, high2 max
    for _ in range(10000):
        d = random.randint(HALF, B-1)
        high1 = random.randint(max(0, d-50), d-1)
        high2 = random.randint(B-50, B-1)
        high = high1*B + high2
        pairs.append((high, d))
    print(f"GM-branch pairs (high1<d): {len(pairs)}")
    for K in range(48, 65):
        fails = 0; first = None
        for high, d in pairs:
            high1 = high // B
            if high1 >= d:   # not GM branch; skip
                continue
            trueq = high // d
            qh = gm_qh(K, d, high)
            if qh != trueq:
                fails += 1
                if first is None:
                    first = (high, d, qh, trueq, high1, high%B)
        print(f"K={K}: fails={fails}" + (f"  firstfail={first}" if first else ""))

search()
