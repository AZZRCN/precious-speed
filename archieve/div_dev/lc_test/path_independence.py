"""
Path independence verification for div.cpp.
Forces each division path to be triggered and verified.
Paths:
1. absDivBasicCore: len2 <= 64 || (len1-len2) <= 64
2. absDivNewtonCore1: len1 < len2*2 (and not basic)
3. absDivMu: len1 >= 2*len2, mu_in <= len2, ab_safe, mu_in >= 64
4. absDivNewtonCore2: len1 >= 2*len2, not absDivMu
"""
import subprocess
import sys
import random
from pathlib import Path

sys.set_int_max_str_digits(2000000)

CUR_DIV = Path(r"d:\precious_speed\cur_div.exe")
TMP_IN = Path(r"d:\precious_speed\div_dev\lc_test\path_ind.in")

BASE = 10000


def run_batch(cases):
    with open(TMP_IN, "w") as f:
        f.write(f"{len(cases)}\n")
        for a, b in cases:
            f.write(f"{a} {b}\n")
    try:
        r = subprocess.run([str(CUR_DIV)], stdin=open(TMP_IN, "rb"),
                           capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        return None, "TIMEOUT"
    if r.returncode != 0:
        return None, f"rc={r.returncode}"
    lines = r.stdout.decode().strip().split("\n")
    results = []
    for line in lines:
        parts = line.split()
        if len(parts) >= 2:
            results.append((parts[0], parts[1]))
        else:
            results.append(("", ""))
    return results, None


def check(a, b, mq, mr):
    q, r = divmod(a, b)
    return mq == str(q) and mr == str(r)


def gen_path_cases():
    """Generate cases targeting each path."""
    cases = []

    # === Path 1: absDivBasicCore (len2 <= 64) ===
    # BASE=10^4, so len2 <= 64 means b has <= 256 decimal digits
    for b_len in [2, 4, 8, 16, 32, 48, 64]:
        for _ in range(20):
            rng = random.Random(b_len * 1000 + _)
            b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
            q_len = rng.randint(1, b_len)
            q = rng.randint(1, BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
            r = rng.randint(0, b - 1)
            a = q * b + r
            cases.append((a, b, f"Basic b_len={b_len}"))

    # === Path 1b: absDivBasicCore (len1-len2 <= 64, but len2 > 64) ===
    for b_len in [100, 200, 500]:
        for delta in [1, 2, 4, 8, 16, 32, 64]:
            for _ in range(5):
                rng = random.Random(b_len * 2000 + delta * 100 + _)
                b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
                q = rng.randint(1, BASE ** delta - 1) if delta > 1 else rng.randint(1, BASE - 1)
                r = rng.randint(0, b - 1)
                a = q * b + r
                cases.append((a, b, f"Basic-quotshort b_len={b_len} quot_len={delta}"))

    # === Path 2: absDivNewtonCore1 (len1 < 2*len2, len2 > 64) ===
    for b_len in [100, 200, 500, 1000]:
        for ratio in [1.1, 1.3, 1.5, 1.7, 1.9]:
            q_len = int(b_len * ratio) - b_len  # len1 = b_len + q_len < 2*b_len
            if q_len <= 64:
                continue  # would go to BasicCore
            for _ in range(10):
                rng = random.Random(int(b_len * 3000 + ratio * 1000) + _)
                b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
                q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
                r = rng.randint(0, b - 1)
                a = q * b + r
                cases.append((a, b, f"Core1 b_len={b_len} ratio={ratio}"))

    # === Path 3: absDivMu (len1 >= 2*len2, mu_in >= 64) ===
    # Need large enough numbers: len2 > 64, len1 >= 2*len2
    for b_len in [100, 200, 500, 1000]:
        for q_mult in [2, 3, 5, 10]:
            q_len = b_len * q_mult
            for _ in range(10):
                rng = random.Random(int(b_len * 4000 + q_mult * 100) + _)
                b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
                q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1)
                r = rng.randint(0, b - 1)
                a = q * b + r
                cases.append((a, b, f"Mu b_len={b_len} q_mult={q_mult}"))

    # === Path 4: absDivNewtonCore2 (len1 >= 2*len2, not absDivMu) ===
    # absDivMu requires mu_in >= 64; for small len2 (< 128), mu_in may be < 64
    for b_len in [70, 80, 90, 100, 120, 150]:
        for q_mult in [2, 3, 5]:
            q_len = b_len * q_mult
            for _ in range(10):
                rng = random.Random(int(b_len * 5000 + q_mult * 100) + _)
                b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
                q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1)
                r = rng.randint(0, b - 1)
                a = q * b + r
                cases.append((a, b, f"Core2 b_len={b_len} q_mult={q_mult}"))

    # === Edge: exact 2*len2 boundary ===
    for b_len in [100, 200, 500, 1000]:
        q_len = b_len  # len1 = 2*len2 exactly
        for _ in range(10):
            rng = random.Random(b_len * 6000 + _)
            b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
            q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1)
            r = rng.randint(0, b - 1)
            a = q * b + r
            cases.append((a, b, f"Boundary-2x b_len={b_len}"))

    return cases


def main():
    cases = gen_path_cases()
    print(f"Generated {len(cases)} path-targeted cases. Running cur_div...", flush=True)

    batch_size = 50
    total = 0
    passed = 0
    failed = []

    for start in range(0, len(cases), batch_size):
        batch = cases[start:start + batch_size]
        results, err = run_batch([(a, b) for a, b, _ in batch])
        if err:
            print(f"  batch {start}: RUN ERROR {err}", flush=True)
            for i in range(len(batch)):
                failed.append((start + i, batch[i], None))
                total += 1
            continue

        for i, (a, b, desc) in enumerate(batch):
            total += 1
            idx = start + i
            mq, mr = results[i] if i < len(results) else ("", "")
            if check(a, b, mq, mr):
                passed += 1
            else:
                failed.append((idx, batch[i], (mq, mr)))
                q, r = divmod(a, b)
                print(f"  FAIL #{idx} [{desc}] |A|={len(str(a))} |B|={len(str(b))}", flush=True)
                print(f"    expected Q={str(q)[:30]}... R={str(r)[:30]}", flush=True)
                print(f"    got      Q={mq[:30]}... R={mr[:30]}", flush=True)

        if (start // batch_size) % 2 == 0:
            print(f"  batch {start}-{start+len(batch)-1}: {passed}/{total}", flush=True)

    print(f"\n=== Path Independence Summary ===")
    print(f"Total: {total}, Passed: {passed}, Failed: {len(failed)}")
    if failed:
        print("\nFAILED cases:")
        for idx, (a, b, desc), res in failed[:20]:
            print(f"  #{idx} [{desc}] |A|={len(str(a))} |B|={len(str(b))}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
