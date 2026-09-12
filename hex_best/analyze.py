import subprocess, sys
gen = sys.argv[1]; seed = sys.argv[2]; divbin = sys.argv[3]
out = subprocess.run([gen, seed], capture_output=True, text=True).stdout
lines = out.split("\n")
T = int(lines[0])
cases = []
i = 1
for _ in range(T):
    if i >= len(lines): break
    p = lines[i].split()
    if len(p) < 2: i += 1; continue
    cases.append((p[0], p[1])); i += 1
rows = []
for idx in range(T):
    A, B = cases[idx]
    inp = "1\n" + A + " " + B + "\n"
    r = subprocess.run([divbin], input=inp, capture_output=True, text=True, timeout=30)
    sp = r.stdout.split()
    fail = False
    if len(sp) >= 2:
        a = int(A, 16); b = int(B, 16)
        if int(sp[0], 16) != a // b or int(sp[1], 16) != a % b:
            fail = True
    else:
        fail = True
    rows.append((idx, len(A), len(B), fail))
# print boundary context
print("idx lenA lenB fail")
for idx, la, lb, f in rows:
    if 505 <= idx <= 1005:
        print(idx, la, lb, "F" if f else ".")
# summary: distinct (lenA,lenB) among fails
from collections import Counter
fc = Counter((la, lb) for idx, la, lb, f in rows if f)
print("FAIL distinct (lenA,lenB) count =", len(fc))
for (la, lb), c in sorted(fc.items())[:20]:
    print("  lenA", la, "lenB", lb, "x", c)
