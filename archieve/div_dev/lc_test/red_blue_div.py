"""
Red-Blue adversarial test for div.cpp.
Red team: generate test cases targeting specific vulnerabilities.
Blue team: cur_div.exe.
Focus: precision strikes on code weak points, not volume.
"""
import subprocess
import sys
import os
from pathlib import Path

sys.set_int_max_str_digits(2000000)

CUR_DIV = Path(r"d:\precious_speed\cur_div.exe")
TMP_IN = Path(r"d:\precious_speed\div_dev\lc_test\rb_div.in")

BASE = 10000  # moptm BASE = 10^4


def run_batch(cases):
    """Run cur_div on batch. Returns list of (q_str, r_str) or None on failure."""
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


def check(case_idx, a, b, mq, mr):
    """Check if cur_div output matches Python divmod."""
    q, r = divmod(a, b)
    eq, er = str(q), str(r)
    if mq == eq and mr == er:
        return True, None
    info = f"|A|={len(str(a))} |B|={len(str(b))}"
    if mq != eq:
        min_len = min(len(mq), len(eq))
        diff_pos = -1
        for i in range(min_len):
            if mq[i] != eq[i]:
                diff_pos = i
                break
        if diff_pos < 0:
            diff_pos = min_len
        info += f" Q diff@{diff_pos}({diff_pos/max(1,min_len)*100:.0f}%) mod_len={len(mq)} cor_len={len(eq)}"
    if mr != er:
        info += f" R diff mod={mr[:20]} cor={er[:20]}"
    return False, info


def gen_red_cases():
    """Generate precision-strike test cases targeting specific vulnerabilities."""
    cases = []

    # === Vulnerability 1: r=0 boundary (product偏大1, tprod[len2]=window[len2]) ===
    for b_high in [BASE // 2, BASE // 2 + 1, BASE - 1, BASE // 2 - 1]:
        for q_val in [BASE - 1, BASE, BASE + 1, BASE * BASE - 1]:
            b = b_high * BASE + 1
            a = q_val * b + (b - 1)
            cases.append((a, b, f"r=0 boundary b_high={b_high} q={q_val}"))

    # === Vulnerability 2: 归一化边界 (divisor高位 = BASE/2) ===
    for b_digits in range(2, 20):
        b = (BASE // 2) * (BASE ** (b_digits - 1))
        for _ in range(10):
            import random
            rng = random.Random(b_digits * 100 + _)
            q = rng.randint(1, BASE ** b_digits)
            r = rng.randint(0, b - 1)
            a = q * b + r
            cases.append((a, b, f"normalized b_high=BASE/2 digits={b_digits}"))

    # === Vulnerability 3: 块边界 (len1 = k * in) ===
    for b_len in [4, 8, 16, 32, 64, 128]:
        for q_mult in [1, 2, 3, 5, 10]:
            b = (BASE ** b_len) - 1
            q = q_mult * (BASE ** b_len) - 1
            r = b - 1
            a = q * b + r
            cases.append((a, b, f"block boundary b_len={b_len} q_mult={q_mult}"))

    # === Vulnerability 4: 路径选择边界 (len1 = 2*len2) ===
    for b_len in [10, 20, 50, 100]:
        b = (BASE ** b_len) - 1
        for delta in [-1, 0, 1]:
            q_len = 2 * b_len + delta
            if q_len < 1:
                continue
            q = (BASE ** q_len) - 1
            r = b // 2
            a = q * b + r
            cases.append((a, b, f"path boundary len1={q_len+b_len} len2={b_len} delta={delta}"))

    # === Vulnerability 5: 商 = BASE^k - 1 (全9999) ===
    for k in [2, 4, 8, 16, 32, 64, 128, 256]:
        b = BASE ** k - 1
        q = BASE ** k - 1
        for r_val in [0, 1, b // 2, b - 1]:
            a = q * b + r_val
            cases.append((a, b, f"q=BASE^k-1 k={k} r={r_val}"))

    # === Vulnerability 6: 余数边界 ===
    for b_len in [5, 10, 20, 50]:
        b = (BASE ** b_len) // 3 + 7
        q = (BASE ** b_len) * 7 + 123
        for r_val in [0, 1, 2, b - 2, b - 1]:
            a = q * b + r_val
            cases.append((a, b, f"remainder boundary b_len={b_len} r={r_val}"))

    # === Vulnerability 7: divisor = BASE^k (最高位1, 其余0) ===
    for k in [2, 4, 8, 16, 32, 64]:
        b = BASE ** k
        q = BASE ** (k + 1) - 1
        r = b - 1
        a = q * b + r
        cases.append((a, b, f"divisor=BASE^k k={k}"))

    # === Vulnerability 8: A = B (商=1, 余数=0) ===
    for k in [2, 4, 8, 16, 32, 64, 128, 256]:
        b = BASE ** k - 1
        a = b
        cases.append((a, b, f"A=B k={k}"))

    # === Vulnerability 9: A = B-1 (商=0, 余数=B-1) ===
    for k in [2, 4, 8, 16, 32, 64, 128, 256]:
        b = BASE ** k - 1
        a = b - 1
        cases.append((a, b, f"A=B-1 k={k}"))

    # === Vulnerability 10: A = 2*B-1 (商=1, 余数=B-1) ===
    for k in [2, 4, 8, 16, 32, 64, 128, 256]:
        b = BASE ** k - 1
        a = 2 * b - 1
        cases.append((a, b, f"A=2B-1 k={k}"))

    # === Vulnerability 11: cyclic路径切换边界 ===
    for b_len in [3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 30, 40, 50]:
        b = (BASE ** b_len) // 2 + 1
        q = (BASE ** (2 * b_len)) - 1
        r = b - 1
        a = q * b + r
        cases.append((a, b, f"cyclic boundary b_len={b_len}"))

    # === Vulnerability 12: FFT精度边界 (大数高位接近BASE/2) ===
    import random
    for seed in range(50):
        rng = random.Random(seed + 99999)
        b_len = rng.randint(50, 500)
        b = (BASE // 2) * (BASE ** (b_len - 1)) + rng.randint(0, BASE ** (b_len - 1))
        q_len = rng.randint(b_len, 2 * b_len)
        q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
        r = rng.randint(0, b - 1)
        a = q * b + r
        cases.append((a, b, f"FFT precision seed={seed} b_len={b_len}"))

    # === Vulnerability 13: 商的连续块全为BASE-1 (全9999) ===
    for b_len in [8, 16, 32]:
        b = (BASE ** b_len) // 2 + 1
        q_blocks = 5
        q = 0
        for i in range(q_blocks):
            q = q * BASE + (BASE - 1)
        r = b - 1
        a = q * b + r
        cases.append((a, b, f"q all-9999 b_len={b_len} blocks={q_blocks}"))

    # === Vulnerability 14: divisor高位 = BASE-1 (最大归一化) ===
    for b_len in [5, 10, 20, 50, 100]:
        b = (BASE - 1) * (BASE ** (b_len - 1)) + (BASE ** (b_len - 1)) - 1
        q = (BASE ** (b_len + 1)) - 1
        for r_val in [0, 1, b // 2, b - 1]:
            a = q * b + r_val
            cases.append((a, b, f"divisor high=BASE-1 b_len={b_len} r={r_val}"))

    # === Vulnerability 15: FFT 舍入边界 (大数全 9999 进位链) ===
    for b_len in [64, 128, 256]:
        b = (BASE ** b_len) - 1
        q = (BASE ** (2 * b_len)) - 1
        r = b - 1
        a = q * b + r
        cases.append((a, b, f"FFT carry chain b_len={b_len}"))

    # === Vulnerability 16: 块边界 + 余数=除数-1 (借位传播极限) ===
    for b_len in [4, 8, 16, 32]:
        b = (BASE ** b_len) // 2 + 1
        q = (BASE ** b_len) * 3 // 2
        r = b - 1
        a = q * b + r
        cases.append((a, b, f"block+max remainder b_len={b_len}"))

    # === Vulnerability 17: 截断 BUG 触发 (b=10^(4k)-1, q=10^(4(k+1))-1, r=1) ===
    for b_len in [70, 80, 90, 100, 120, 150, 200]:
        b = 10 ** (4 * b_len) - 1
        q = 10 ** (4 * (b_len + 1)) - 1
        for r_val in [0, 1, 2, 7, b // 2, b - 1]:
            a = q * b + r_val
            cases.append((a, b, f"truncation trigger b_len={b_len} r={r_val}"))

    # === Vulnerability 18: 随机大规模 ===
    for seed in range(100):
        rng = random.Random(seed + 77777)
        b_len = rng.randint(100, 1000)
        b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
        q_len = rng.randint(b_len, 3 * b_len)
        q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
        r = rng.randint(0, b - 1)
        a = q * b + r
        cases.append((a, b, f"random large seed={seed} b_len={b_len}"))

    return cases


def main():
    round_num = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    cases = gen_red_cases()
    # Round 2-5: add extra random cases with different seeds
    import random
    extra_seed_base = round_num * 12345
    for seed in range(200):
        rng = random.Random(seed + extra_seed_base)
        b_len = rng.randint(2, 500)
        b = rng.randint(BASE ** (b_len - 1) if b_len > 1 else 1, BASE ** b_len - 1)
        if b == 0:
            b = 1
        q_len = rng.randint(1, 3 * b_len)
        q = rng.randint(BASE ** (q_len - 1) if q_len > 1 else 1, BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
        r = rng.randint(0, b - 1)
        a = q * b + r
        cases.append((a, b, f"round{round_num}_extra seed={seed} b_len={b_len}"))
    print(f"Round {round_num}: Generated {len(cases)} red-team cases. Running blue-team (cur_div)...", flush=True)

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
            ok, info = check(idx, a, b, mq, mr)
            if ok:
                passed += 1
            else:
                failed.append((idx, batch[i], (mq, mr)))
                print(f"  FAIL #{idx} [{desc}] {info}", flush=True)

        if (start // batch_size) % 2 == 0:
            print(f"  batch {start}-{start+len(batch)-1}: {passed}/{total}", flush=True)

    print(f"\n=== Red-Blue Summary ===")
    print(f"Total: {total}, Passed: {passed}, Failed: {len(failed)}")
    if failed:
        print("\nFAILED cases:")
        for idx, (a, b, desc), res in failed[:20]:
            print(f"  #{idx} [{desc}] |A|={len(str(a))} |B|={len(str(b))}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
