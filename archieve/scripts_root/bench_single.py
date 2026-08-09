"""Generate single-size test cases and run pure bench."""
import random
import subprocess
import sys

sys.set_int_max_str_digits(2000000)

BASE = 10000
CUR_DIV = r"d:\precious_speed\cur_div_pure.exe"
TMP_IN = r"d:\precious_speed\single_bench.in"

sizes = [
    ("medium_500", 500, 500, 20),
    ("medium_1k", 1000, 1000, 10),
    ("large_5k", 5000, 5000, 5),
    ("large_10k", 10000, 10000, 3),
    ("xlarge_50k", 50000, 50000, 1),
    ("xlarge_100k", 100000, 100000, 1),
]

for name, b_len, q_len, n in sizes:
    cases = []
    for i in range(n):
        rng = random.Random(b_len * 10000 + q_len * 100 + i)
        b = rng.randint(BASE ** (b_len - 1), BASE ** b_len - 1)
        q = rng.randint(BASE ** (q_len - 1), BASE ** q_len - 1) if q_len > 1 else rng.randint(1, BASE - 1)
        r = rng.randint(0, b - 1)
        a = q * b + r
        cases.append((a, b))

    with open(TMP_IN, "w") as f:
        f.write(f"{len(cases)}\n")
        for a, b in cases:
            f.write(f"{a} {b}\n")

    with open(TMP_IN, "rb") as f:
        data = f.read()

    # Run 3 times, take min
    times = []
    for _ in range(3):
        r = subprocess.run([CUR_DIV], input=data, capture_output=True, timeout=120)
        line = r.stderr.decode().strip()
        # Parse "[DIV] pure div time: X.XXX ms"
        t = float(line.split(":")[1].strip().replace("ms", "").strip())
        times.append(t)

    print(f"{name} (b={b_len} q={q_len} n={n}): min={min(times):.3f}ms avg={sum(times)/len(times):.3f}ms")
