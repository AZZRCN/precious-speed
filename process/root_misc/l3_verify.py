import sys
def parse_cases(fn):
    lines = open(fn).read().split('\n')
    T = int(lines[0])
    cases = []
    i = 1
    for _ in range(T):
        if i >= len(lines): break
        a, b = lines[i].split()
        cases.append((a, b)); i += 1
    return cases
cases = parse_cases(sys.argv[1])
out = [l for l in open(sys.argv[2]).read().split('\n')]
oi = 0
ok = 0
for (a, b) in cases:
    A = int(a, 16); B = int(b, 16)
    eq = (A // B, A % B)
    while oi < len(out) and out[oi].strip() == '': oi += 1
    if oi >= len(out):
        print("MISSING OUTPUT"); break
    q, r = out[oi].split(); oi += 1
    if int(q, 16) == eq[0] and int(r, 16) == eq[1]:
        ok += 1
    else:
        print(f"MISMATCH a={a[:24]}... b={b[:24]}... exp=({eq[0]:x},{eq[1]:x}) got=({q},{r})")
        break
print(f"verified {ok}/{len(cases)}")
sys.exit(0 if ok == len(cases) else 1)
