#!/usr/bin/env python3
"""
VM-side benchmark for MUL big-int candidates.

- Compiles each candidate with LC-like flags (x86-64-v3 = AVX2+FMA+BMI2, NO AVX-512,
  mirroring LC's Zen3 where AVX-512 is disabled).
- Correctness (two tiers):
    * ORACLE tier: cases present in oracle_cache.pkl are checked against a Python
      int(a)*int(b) ground truth (int-list compare -> format-agnostic).
    * CONSENSUS tier: remaining heavy cases are checked by cross-binary agreement
      (all 4 binaries emit byte-identical output). Definitive oracle for those is
      deferred to LC submission (most authoritative).
- Timing: single-core (taskset -c 0), CPU-time via rusage(RUSAGE_CHILDREN),
  warmup + interleaved alternating multi-round; per-case MIN + LC-style worst
  (max over cases of per-case MIN).

Run on VM:  python3 vm_bench.py
"""
import os, sys, subprocess, resource, json, time, pickle

sys.set_int_max_str_digits(10_000_000)

BASE = "/home/azzr/mulbench"
CASES_DIR = os.path.join(BASE, "cases")
SRC_DIR = os.path.join(BASE, "src")
BIN_DIR = os.path.join(BASE, "bin")
ORACLE_PKL = os.path.join(BASE, "oracle_cache.pkl")
# x86-64-v3 = AVX2 + FMA + BMI2, NO AVX-512 -> matches LC (Zen3, AVX-512 CE'd)
FLAGS = "-O2 -std=c++23 -march=x86-64-v3"

# (label, src filename)
BINS = [
    ("gold_385663", "mul_gold.cpp"),     # the 36ms holder (best/mul_385663.cpp)
    ("best_cpp",    "mul_best.cpp"),     # best/mul.cpp (206KB variant)
    ("r4_387374",   "mul_387374.cpp"),   # best/mul_387374.cpp (hint-lib radix-4 auto-vec)
    ("r4_current",  "mul_r4.cpp"),       # current product (fused radix-4)
]
GOLD = "gold_385663"

ROUNDS = 30
WARMUP = 4


def compile_all():
    os.makedirs(BIN_DIR, exist_ok=True)
    res = {}
    for label, src in BINS:
        out = os.path.join(BIN_DIR, label)
        p = subprocess.run(["g++", *FLAGS.split(), "-o", out,
                            os.path.join(SRC_DIR, src)],
                           capture_output=True, text=True)
        res[label] = (p.returncode, p.stderr.strip()[:500])
    return res


def run_bin(binpath, casepath, capture=True):
    stdin = open(casepath, "rb")
    if capture:
        p = subprocess.run(["taskset", "-c", "0", binpath], stdin=stdin,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=300)
        return p.returncode, p.stdout
    b = resource.getrusage(resource.RUSAGE_CHILDREN)
    subprocess.run(["taskset", "-c", "0", binpath], stdin=stdin,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=300)
    a = resource.getrusage(resource.RUSAGE_CHILDREN)
    return 0, (a.ru_utime + a.ru_stime) - (b.ru_utime + b.ru_stime)


def ints_of(raw):
    return [int(x) for x in raw.split()]


def main():
    t0 = time.time()
    cres = compile_all()
    print("=== COMPILE (flags: %s) ===" % FLAGS)
    for label, (rc, err) in cres.items():
        print(f"  {label:14}: rc={rc}" + (f"  ERR={err}" if rc else "  OK"))
    if any(rc for rc, _ in cres.values()):
        print("COMPILE FAILED -> abort"); sys.exit(1)

    cases = sorted(f for f in os.listdir(CASES_DIR) if f.endswith(".in"))
    print(f"=== CASES: {len(cases)} ===")

    oracle = {}
    if os.path.exists(ORACLE_PKL):
        with open(ORACLE_PKL, "rb") as f:
            oracle = pickle.load(f)
        print(f"=== ORACLE cache: {len(oracle)} cases ===")
    else:
        print("=== ORACLE cache: MISSING (run gen_oracle.py first) ===")

    binpaths = {label: os.path.join(BIN_DIR, label) for label, _ in BINS}

    # ---- correctness: capture each binary's output once ----
    print("=== CORRECTNESS ===")
    out_bytes = {label: {} for label, _ in BINS}
    for label, _ in BINS:
        for c in cases:
            cp = os.path.join(CASES_DIR, c)
            rc, raw = run_bin(binpaths[label], cp, capture=True)
            if rc != 0:
                print(f"  [RUN-ERR] {label} {c} rc={rc}")
            out_bytes[label][c] = raw

    corr = {}
    for label, _ in BINS:
        ok_o = ok_c = bad = 0
        bad_cases = []
        for c in cases:
            raw = out_bytes[label][c]
            if c in oracle:
                if ints_of(raw) == oracle[c]:
                    ok_o += 1
                else:
                    bad += 1; bad_cases.append(c + "(oracle)")
            else:
                # consensus: compare to gold's bytes
                if raw == out_bytes[GOLD][c]:
                    ok_c += 1
                else:
                    bad += 1; bad_cases.append(c + "(consensus)")
        corr[label] = {"oracle_ok": ok_o, "consensus_ok": ok_c,
                       "bad": bad, "bad_cases": bad_cases}
        print(f"  {label:14}: oracle {ok_o:2}  consensus {ok_c:2}  BAD={bad}" +
              (f"  {bad_cases}" if bad else ""))

    # r4_current vs gold byte-identical on every case?
    r4vsgold = all(out_bytes["r4_current"][c] == out_bytes[GOLD][c] for c in cases)
    print(f"  r4_current == gold_385663 byte-identical on all {len(cases)} cases: {r4vsgold}")

    # ---- benchmark: warmup + interleaved alternating, CPU-time ----
    print(f"=== BENCH (warmup={WARMUP} rounds={ROUNDS}, taskset -c 0, CPU-time) ===")
    data = {c: {label: [] for label, _ in BINS} for c in cases}
    for r in range(WARMUP + ROUNDS):
        is_warm = r < WARMUP
        k = r % len(BINS)
        order = BINS[k:] + BINS[:k]
        for c in cases:
            cp = os.path.join(CASES_DIR, c)
            for label, _ in order:
                _rc, t = run_bin(binpaths[label], cp, capture=False)
                if not is_warm:
                    data[c][label].append(t)
        if (r + 1) % 5 == 0:
            print(f"  [round {r+1}/{WARMUP+ROUNDS}] elapsed {time.time()-t0:.1f}s")

    summary = {}
    print(f"\n{'case':22}" + "".join(f"{label:>15}" for label, _ in BINS))
    for c in cases:
        line = f"{c:22}"
        summary[c] = {}
        for label, _ in BINS:
            ts = sorted(data[c][label])
            mn = ts[0]; md = ts[len(ts)//2]
            summary[c][label] = {"min_ms": mn*1000, "median_ms": md*1000, "n": len(ts)}
            line += f"{mn*1000:15.3f}"
        print(line)

    worst = {label: max(summary[c][label]["min_ms"] for c in cases) for label, _ in BINS}
    print("\n=== LC-style worst-case = max over cases of per-case MIN (ms) ===")
    for label, _ in BINS:
        print(f"  {label:14}: {worst[label]:8.3f} ms")
    g = worst[GOLD]
    print(f"\n=== speedup vs gold_385663 (LC 36ms holder) ===")
    for label, _ in BINS:
        if label == GOLD:
            continue
        print(f"  {label:14}: {worst[label]:8.3f} ms  ({g/worst[label]:.3f}x)")

    out = {"flags": FLAGS, "correctness": corr, "r4_eq_gold": r4vsgold,
           "worst_ms": worst, "per_case": summary, "rounds": ROUNDS, "warmup": WARMUP}
    with open(os.path.join(BASE, "bench_result.json"), "w") as f:
        json.dump(out, f, indent=2)
    print(f"\nWROTE bench_result.json  (total elapsed {time.time()-t0:.1f}s)")


if __name__ == "__main__":
    main()
