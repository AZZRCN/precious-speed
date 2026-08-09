"""Compare different INV_NEWTON_BASE_THRESHOLD values."""
import subprocess
import sys
import random

sys.set_int_max_str_digits(2000000)

BASE = 10000
EXES = {
    "default(64)": r"d:\precious_speed\cur_div_pure.exe",
    "inv128": r"d:\precious_speed\cur_div_inv128.exe",
}

TMP_IN = r"d:\precious_speed\cmp_inv.in"

# Generate xlarge case
rng = random.Random(50000 * 10000 + 50000 * 100)
b = rng.randint(BASE ** 49999, BASE ** 50000 - 1)
q = rng.randint(BASE ** 49999, BASE ** 50000 - 1)
r = rng.randint(0, b - 1)
a = q * b + r

with open(TMP_IN, "w") as f:
    f.write(f"1\n{a} {b}\n")

with open(TMP_IN, "rb") as f:
    data = f.read()

for name, exe in EXES.items():
    times = []
    for _ in range(5):
        r = subprocess.run([exe], input=data, capture_output=True, timeout=120)
        line = r.stderr.decode().strip()
        t = float(line.split(":")[1].strip().replace("ms", "").strip())
        times.append(t)
    print(f"{name}: min={min(times):.3f}ms avg={sum(times)/len(times):.3f}ms")
