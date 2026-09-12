#!/usr/bin/env python3
"""Generate Python int-multiplication oracle for selected cases. Caches to pickle.

Decimal->int in CPython is O(n^2) and 2M-digit parsing is slow (~minutes), so we
compute the oracle only for LIGHT cases (fast, definitive) + a few HEAVY spot-checks.
Remaining heavy cases are verified by cross-binary consensus in vm_bench.py.

Run on VM:  python3 gen_oracle.py            # default set (light + 2 heavy spots)
            python3 gen_oracle.py max_max_00 fft_killer_00   # explicit
"""
import os, sys, pickle, time, argparse

sys.set_int_max_str_digits(10_000_000)  # allow 2M-digit I/O

BASE = "/home/azzr/mulbench"
CASES = os.path.join(BASE, "cases")
OUT = os.path.join(BASE, "oracle_cache.pkl")

# default: all light cases + 2 heavy spot-checks (definitive). Heavy rest -> consensus.
DEFAULT = [
    "example_00.in", "small_00.in",
    "medium_00.in", "medium_01.in", "medium_02.in",
    "large_00.in", "large_01.in", "large_02.in",
    "zero_00.in", "large_small_00.in",
    "max_max_00.in", "fft_killer_00.in",
]


def oracle_ints(cp):
    """Parse T then T 'a b' pairs; return [int(a)*int(b), ...]."""
    with open(cp) as f:
        toks = []
        for line in f:
            toks.extend(line.split())
    T = int(toks[0])
    out = []
    idx = 1
    for _ in range(T):
        a = toks[idx]; b = toks[idx + 1]; idx += 2
        out.append(int(a) * int(b))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cases", nargs="*", default=None, help="case names; default=light+spots")
    args = ap.parse_args()
    targets = args.cases if args.cases else DEFAULT

    existing = {}
    if os.path.exists(OUT):
        with open(OUT, "rb") as f:
            existing = pickle.load(f)
        print(f"loaded existing cache: {len(existing)} cases")

    t0 = time.time()
    done = 0
    for c in targets:
        cp = os.path.join(CASES, c)
        if not os.path.exists(cp):
            print(f"SKIP (missing) {c}")
            continue
        if c in existing:
            print(f"cached {c}")
            continue
        sz = os.path.getsize(cp)
        ts = time.time()
        existing[c] = oracle_ints(cp)
        dt = time.time() - ts
        done += 1
        print(f"  {c:22} size={sz:>10} T={len(existing[c]):>7} oracle {dt:7.1f}s  (total {time.time()-t0:.1f}s)", flush=True)
        # incremental save so a slow heavy case can't lose prior work
        with open(OUT, "wb") as f:
            pickle.dump(existing, f)
    print(f"WROTE {OUT}  ({len(existing)} cases cached, {done} newly computed, {time.time()-t0:.1f}s total)", flush=True)


if __name__ == "__main__":
    main()
