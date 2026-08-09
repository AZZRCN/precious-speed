
import math, os, sys, itertools
from collections import defaultdict

PEN = {3: 1.15, 5: 1.25, 7: 1.35}
MINBLK = 64

def gen(radices, maxst):
    out = [(1, ())]
    for n in range(1, maxst + 1):
        for combo in itertools.combinations_with_replacement(radices, n):
            c = 1
            for r in combo:
                c *= r
            out.append((c, combo))
    best = {}
    for c, combo in out:
        cost = sum(PEN[r] * math.log2(r) for r in combo)
        if c not in best or cost < best[c][0]:
            best[c] = (cost, combo)
    return sorted((c, v[0]) for c, v in best.items())

def make(table):
    def f(n):
        best = None
        for c, extra in table:
            v, k = c, 0
            while v < n:
                v <<= 1; k += 1
            if c > 1 and (1 << k) < MINBLK:
                continue
            cost = v * (k + extra)
            if best is None or cost < best[0]:
                best = (cost, v, c)
        return best
    return f

LAD = {
    "P2":    make(gen([], 0)),
    "P23":   make(gen([3], 1)),
    "3x2":   make(gen([3], 2)),
    "35x1":  make(gen([3, 5], 1)),
    "35x2":  make(gen([3, 5], 2)),
    "357x2": make(gen([3, 5, 7], 2)),
    "357x3": make(gen([3, 5, 7], 3)),
}
names = list(LAD)

CASES = sys.argv[1:]
print(f"{'case':<28}" + "".join(f"{k:>9}" for k in names) + f"{'IDEAL':>9}")
print("-" * (28 + 9 * (len(names) + 1)))
grand = defaultdict(float)
detail = {}
for case in CASES:
    cp, fp = f"/tmp/ceil_{case}.csv", f"/tmp/fh_{case}.csv"
    if not (os.path.exists(cp) and os.path.exists(fp)):
        print(f"{case:<28} (missing)"); continue
    # 决策映射: used -> 绑定 need (取该档位下出现过的最大 need)
    bind = {}
    with open(cp) as f:
        next(f)
        for line in f:
            _, need, used, _ = line.split(",")
            need, used = int(need), int(used)
            if need < 2: continue
            bind[used] = max(bind.get(used, 0), need)
    tot = defaultdict(float)
    rows = []
    with open(fp) as f:
        next(f)
        for line in f:
            L, cnt = map(int, line.split(","))
            if L < 2: continue
            need = bind.get(L, L)         # 没有决策记录时保守取 L 本身
            tot["IDEAL"] += need * math.log2(need) * cnt
            base_cost = L * math.log2(L) * cnt
            for nm in names:
                cost, v, c = LAD[nm](need)
                tot[nm] += cost * cnt
            rows.append((base_cost, L, need, cnt))
    base = tot["P23"]
    line = f"{case:<28}"
    for nm in names + ["IDEAL"]:
        grand[nm] += tot[nm]
        line += f"{tot[nm]/base*100:8.1f}%"
    print(line)
    rows.sort(reverse=True)
    detail[case] = rows[:6]
print("-" * (28 + 9 * (len(names) + 1)))
line = f"{'GRAND (vs P23=100)':<28}"
for nm in names + ["IDEAL"]:
    line += f"{grand[nm]/grand['P23']*100:8.1f}%"
print(line)

print("\n--- 每例 top6 变换 (按 NlogN*次数), 及 357x3 会选的档位 ---")
for case, rows in detail.items():
    print(f"  [{case}]")
    for w, L, need, cnt in rows:
        cost, v, c = LAD["357x3"](need)
        e2 = 0; f2 = v
        while f2 % 2 == 0: f2 //= 2; e2 += 1
        print(f"    now len={L:<8} x{cnt:<5} (need={need:<8} waste={100*(L/need-1):5.1f}%)"
              f"  ->357x3 {v:<8}={f2}*2^{e2:<2} waste={100*(v/need-1):5.1f}%")
