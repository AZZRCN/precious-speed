import random
B = 65536
HALF = B//2

def gm_qh(K, d, high):
    m = ((1 << K) // d) + 1
    return (m * high) >> K

def search():
    random.seed(2026)
    # known bad pair (from trace)
    bad = (2238379095, 60071)  # high, d
    # large random sample
    pairs = [bad]
    for _ in range(20000):
        d = random.randint(HALF, B-1)
        high = random.randint(0, B*B-1)
        pairs.append((high, d))
    # also structured: high with high1 just below d
    for _ in range(5000):
        d = random.randint(HALF, B-1)
        high1 = random.randint(max(1, d-2000), d-1)
        high2 = random.randint(0, B-1)
        high = high1*B + high2
        pairs.append((high, d))
    # high1 >= d branch pairs (should be B-1, exact) - include for completeness
    for _ in range(2000):
        d = random.randint(HALF, B-1)
        high1 = random.randint(d, B-1)
        high2 = random.randint(0, B-1)
        high = high1*B + high2
        pairs.append((high, d))
    print(f"total pairs: {len(pairs)}")
    for K in range(48, 65):
        fails = 0
        first = None
        for high, d in pairs:
            high1 = high // B
            trueq = B-1 if high1 >= d else high // d
            qh = gm_qh(K, d, high)
            if qh != trueq:
                fails += 1
                if first is None:
                    first = (high, d, qh, trueq, high1)
        print(f"K={K}: fails={fails}" + (f"  firstfail={first}" if first else ""))

search()
