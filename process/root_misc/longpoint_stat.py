import sys, statistics as st

root = sys.argv[1]
cats = sys.argv[2:]

def load(cat, sol):
    with open(f"{root}/lp_{cat}_{sol}.txt") as f:
        return sorted(int(x) for x in f if x.strip())

def q(v, p):
    # linear-interpolated percentile on sorted list
    if not v: return 0
    k = (len(v)-1) * p
    lo = int(k); hi = min(lo+1, len(v)-1)
    return v[lo] + (v[hi]-v[lo]) * (k-lo)

def ms(x): return x/1e6

rows = []
print(f"{'category':<24} {'sol':<5} {'n':>3} {'min':>8} {'p25':>8} {'med':>8} {'p75':>8} {'max':>8} {'mean':>8} {'sd':>7}")
print("-"*104)
for cat in cats:
    try:
        o = load(cat, "orig"); p = load(cat, "opt")
    except FileNotFoundError:
        continue
    for name, v in (("orig", o), ("opt", p)):
        print(f"{cat:<24} {name:<5} {len(v):>3} {ms(min(v)):>8.2f} {ms(q(v,.25)):>8.2f} "
              f"{ms(q(v,.5)):>8.2f} {ms(q(v,.75)):>8.2f} {ms(max(v)):>8.2f} "
              f"{ms(st.mean(v)):>8.2f} {ms(st.pstdev(v)):>7.2f}")
    rows.append((cat, o, p))
    print("-"*104)

print()
print(f"{'category':<24} {'orig_med':>9} {'opt_med':>9} {'delta%':>8} {'speedup':>8} {'min-based':>10} {'separated':>10} {'U-p<0.01':>9}")
print("-"*104)

def mannwhitney_z(a, b):
    # rank-sum with tie correction -> normal approx z
    n1, n2 = len(a), len(b)
    comb = sorted([(v, 0) for v in a] + [(v, 1) for v in b])
    ranks = [0.0]*len(comb)
    i = 0
    ties = []
    while i < len(comb):
        j = i
        while j+1 < len(comb) and comb[j+1][0] == comb[i][0]:
            j += 1
        r = (i+j)/2.0 + 1
        for k in range(i, j+1):
            ranks[k] = r
        ties.append(j-i+1)
        i = j+1
    R1 = sum(ranks[k] for k in range(len(comb)) if comb[k][1] == 0)
    U1 = R1 - n1*(n1+1)/2.0
    mu = n1*n2/2.0
    N = n1+n2
    tie_term = sum(t**3 - t for t in ties)
    var = n1*n2/12.0 * ((N+1) - tie_term/(N*(N-1)))
    if var <= 0: return 0.0
    return (U1-mu)/var**0.5

for cat, o, p in rows:
    om, pm = q(o,.5), q(p,.5)
    delta = (pm-om)/om*100.0
    sp = om/pm if pm else 0
    minsp = min(o)/min(p) if min(p) else 0
    sep = "YES" if q(p,.75) < q(o,.25) else ("partial" if pm < om else "NO")
    z = mannwhitney_z(o, p)
    signif = "YES" if abs(z) > 2.576 else "no"
    print(f"{cat:<24} {ms(om):>9.2f} {ms(pm):>9.2f} {delta:>+7.1f}% {sp:>8.3f} {minsp:>10.3f} {sep:>10} {signif:>9} (z={z:+.1f})")
