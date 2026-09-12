"""Run profile div bench on xlarge case."""
import subprocess
import sys

sys.set_int_max_str_digits(2000000)
import random

BASE = 10000
CUR_DIV = r"d:\precious_speed\cur_div_prof.exe"
TMP_IN = r"d:\precious_speed\prof_xlarge.in"

# Generate 1 xlarge case (b=50000, q=50000)
rng = random.Random(50000 * 10000 + 50000 * 100)
b = rng.randint(BASE ** 49999, BASE ** 50000 - 1)
q = rng.randint(BASE ** 49999, BASE ** 50000 - 1)
r = rng.randint(0, b - 1)
a = q * b + r

with open(TMP_IN, "w") as f:
    f.write(f"1\n{a} {b}\n")

with open(TMP_IN, "rb") as f:
    data = f.read()

r = subprocess.run([CUR_DIV], input=data, capture_output=True, timeout=120)
print("stderr:")
print(r.stderr.decode())
