"""Random regression test: generate random A/B pairs, compare cur_div vs Python divmod."""
import random
import subprocess
import sys
from pathlib import Path

sys.set_int_max_str_digits(100000)

CUR_DIV = Path(r"d:\precious_speed\cur_div.exe")
TMP_IN = Path(r"d:\precious_speed\div_dev\lc_test\rand.in")
TMP_OUT = Path(r"d:\precious_speed\div_dev\lc_test\rand.out")


def gen_case(seed):
    """Generate a random test case. Returns (A_str, B_str, expected_q, expected_r)."""
    rng = random.Random(seed)
    # Mix of sizes: small, medium, large
    size_class = rng.choice(["tiny", "small", "medium", "large", "mixed"])
    if size_class == "tiny":
        alen = rng.randint(1, 5)
        blen = rng.randint(1, 5)
    elif size_class == "small":
        alen = rng.randint(5, 50)
        blen = rng.randint(5, 50)
    elif size_class == "medium":
        alen = rng.randint(50, 500)
        blen = rng.randint(50, 500)
    elif size_class == "large":
        alen = rng.randint(500, 5000)
        blen = rng.randint(500, 5000)
    else:  # mixed
        alen = rng.randint(1, 5000)
        blen = rng.randint(1, 5000)

    # Mostly |A| >= |B|
    if rng.random() < 0.8 and alen < blen:
        alen, blen = blen, alen

    a = int("".join(str(rng.randint(1 if i == 0 else 0, 9)) for i in range(alen)))
    b = int("".join(str(rng.randint(1 if i == 0 else 0, 9)) for i in range(blen)))
    if b == 0:
        b = 1
    q, r = divmod(a, b)
    return str(a), str(b), str(q), str(r)


def run_batch(cases):
    """Run cur_div on a batch of cases. Returns list of (q_str, r_str)."""
    with open(TMP_IN, "w") as f:
        f.write(f"{len(cases)}\n")
        for a, b, _, _ in cases:
            f.write(f"{a} {b}\n")
    r = subprocess.run([str(CUR_DIV)], stdin=open(TMP_IN, "rb"),
                       capture_output=True, timeout=120)
    if r.returncode != 0:
        return None
    lines = r.stdout.decode().strip().split("\n")
    results = []
    for line in lines:
        parts = line.split()
        if len(parts) >= 2:
            results.append((parts[0], parts[1]))
        else:
            results.append(("", ""))
    return results


def main():
    n_cases = int(sys.argv[1]) if len(sys.argv) > 1 else 300
    batch_size = 50
    total = 0
    passed = 0
    failed = []

    print(f"Running {n_cases} random cases in batches of {batch_size}...")
    for batch_start in range(0, n_cases, batch_size):
        batch_n = min(batch_size, n_cases - batch_start)
        cases = [gen_case(batch_start + i) for i in range(batch_n)]
        results = run_batch(cases)
        if results is None:
            print(f"  batch {batch_start}: cur_div FAILED (rc!=0)")
            for i in range(batch_n):
                failed.append((batch_start + i, cases[i], None))
            total += batch_n
            continue

        for i in range(batch_n):
            total += 1
            a, b, eq, er = cases[i]
            mq, mr = results[i] if i < len(results) else ("", "")
            if mq == eq and mr == er:
                passed += 1
            else:
                failed.append((batch_start + i, cases[i], (mq, mr)))
                print(f"  FAIL case {batch_start+i}: |A|={len(a)} |B|={len(b)}")
                print(f"    expected Q={eq[:30]}... R={er[:30]}...")
                print(f"    got      Q={mq[:30]}... R={mr[:30]}...")

        if (batch_start // batch_size) % 2 == 0:
            print(f"  batch {batch_start}-{batch_start+batch_n-1}: done ({passed}/{total})")

    print(f"\n=== Summary ===")
    print(f"Total: {total}, Passed: {passed}, Failed: {len(failed)}")
    if failed:
        print("FAILED cases:")
        for idx, case, result in failed[:10]:
            a, b, eq, er = case
            print(f"  case {idx}: |A|={len(a)} |B|={len(b)}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
