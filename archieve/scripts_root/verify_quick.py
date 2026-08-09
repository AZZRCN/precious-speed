"""Quick correctness verification for div.cpp output."""
import sys

sys.set_int_max_str_digits(2000000)

with open(r"d:\precious_speed\prof_in.txt") as f:
    inputs = f.read().strip().split("\n")
n = int(inputs[0])

with open(r"d:\precious_speed\quick_out.txt") as f:
    outputs = f.read().strip().split("\n")

all_ok = True
for i in range(n):
    a, b = map(int, inputs[i + 1].split())
    q, r = divmod(a, b)
    parts = outputs[i].split()
    q2 = int(parts[0])
    r2 = int(parts[1])
    if q == q2 and r == r2:
        print(f"Case {i}: OK")
    else:
        print(f"Case {i}: FAIL (q_match={q==q2}, r_match={r==r2})")
        all_ok = False

print(f"=== {'ALL OK' if all_ok else 'SOME FAILED'} ===")
