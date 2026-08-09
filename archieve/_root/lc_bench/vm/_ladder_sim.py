
import math, os, sys
from collections import defaultdict

def odd_mults(maxset):
    return sorted(maxset)

def smooth_odds(limit=4096):
    s = []
    for c in range(1, limit, 2):
        x = c
        for p in (3, 5, 7):
            while x % p == 0:
                x //= p
        if x == 1:
            s.append(c)
    return s

LADDERS = {
    "P2":    [1],
    "P23":   [1, 3],
    "P235":  [1, 3, 5],
    "P2357": [1, 3, 5, 7],
    "P2SM":  smooth_odds(),
}
MINLEN = 192   # 非 2 幂顶层的最小长度限制 (与 FFT3_MIN 一致)

def ceil_with(n, odds):
    best = None
    for c in odds:
        # 找最小的 c*2^k >= n
        if c == 1:
            v = 1
            while v < n:
                v <<= 1
        else:
            v = c
            while v < n:
                v <<= 1
            # c*2^k 形式下 k 可为 0, 但要求 >= MINLEN 且是 c*2^k
            if v < MINLEN:
                continue
        if best is None or v < best:
            best = v
    return best

def work(n):
    return n * math.log2(n) if n >= 2 else 0.0

CASES = sys.argv[1:]
print(f"{'case':<28}" + "".join(f"{k:>11}" for k in LADDERS) + f"{'IDEAL':>11}")
print("-" * (28 + 11 * (len(LADDERS) + 1)))
grand = defaultdict(float)
for case in CASES:
    path = f"/tmp/ceil_{case}.csv"
    if not os.path.exists(path):
        print(f"{case:<28} (missing)")
        continue
    tot = defaultdict(float)
    with open(path) as f:
        next(f)
        for line in f:
            tag, need, used, cnt = line.split(",")
            need, cnt = int(need), int(cnt)
            if need < 2:
                continue
            tot["IDEAL"] += work(need) * cnt
            for name, odds in LADDERS.items():
                tot[name] += work(ceil_with(need, odds)) * cnt
    base = tot["P23"]
    row = f"{case:<28}"
    for name in list(LADDERS) + ["IDEAL"]:
        grand[name] += tot[name]
        row += f"{tot[name]/base*100:10.1f}%"
    print(row)
print("-" * (28 + 11 * (len(LADDERS) + 1)))
row = f"{'GRAND (vs P23=100)':<28}"
for name in list(LADDERS) + ["IDEAL"]:
    row += f"{grand[name]/grand['P23']*100:10.1f}%"
print(row)
