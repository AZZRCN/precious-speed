import subprocess, random, sys

EXE = r"D:\precious_speed\hex_best\_cyc_test.exe"
PY = r"C:\Users\Administrator\.workbuddy\binaries\python\versions\3.13.12\python.exe"

random.seed(20260813)

def gen_case(nb, na_ratio):
    nb_limbs_bits = 64 * nb
    # B: nb limbs, top limb nonzero
    B = random.getrandbits(64 * nb)
    if B < (1 << (64 * (nb - 1))):
        B |= (1 << (64 * (nb - 1)))   # ensure nb-limb magnitude
    na = max(3 * nb, int(nb * na_ratio))
    A = random.getrandbits(64 * na)
    # ensure A >= B (A has far more limbs, but force top nonzero)
    if A < (1 << (64 * (na - 1))):
        A |= (1 << (64 * (na - 1)))
    if A < B:
        A += B
    return A, B

cases = []
# sweep nb, vary ratio + boundary
nbs = [200,256,300,400,500,512,640,800,1000,1024,1280,1500,2000,2500,3000]
ratios = [3.0, 5.0, 8.0]
for nb in nbs:
    for r in ratios:
        cases.append((nb, r))
    cases.append((nb, 2.0 + 10.0/nb))   # boundary na≈2nb+10
# a few huge stress
for nb in [4000, 5000, 8000]:
    cases.append((nb, 4.0))

# build input
cases_data = []
inp = [str(len(cases))]
for (nb, r) in cases:
    A, B = gen_case(nb, r)
    cases_data.append((A, B))
    inp.append(format(A, 'X') + " " + format(B, 'X'))

src = "\n".join(inp) + "\n"
with open(r"D:\precious_speed\hex_best\_mu_in.txt", "w") as f:
    f.write(src)

env = {"MU": "1"}
# run
r = subprocess.run([EXE], input=src, capture_output=True, text=True, env={**__import__('os').environ, **env})
if r.returncode != 0:
    print("RUN FAILED rc=", r.returncode)
    print(r.stderr[:2000])
    sys.exit(1)
out_lines = r.stdout.strip().split("\n")
assert len(out_lines) == len(cases_data), (len(out_lines), len(cases_data))

fails = 0
for idx, (A, B) in enumerate(cases_data):
    qexp = A // B
    rexp = A % B
    parts = out_lines[idx].split()
    if len(parts) != 2:
        print(f"CASE {idx} nb={cases[idx][0]} r={cases[idx][1]}: bad output '{out_lines[idx][:60]}'")
        fails += 1
        continue
    qgot = int(parts[0], 16)
    rgot = int(parts[1], 16)
    nb_, r_ = cases[idx]
    na_ = max(3*nb_, int(nb_*r_))
    qn_ = na_ - nb_
    in_ = max(1, nb_//2)
    if qgot != qexp or rgot != rexp:
        fails += 1
        print(f"DIFF case {idx:2d} nb={nb_:5d} r={r_:.3f} na={na_:6d} qn={qn_:6d} in={in_:5d} "
              f"qn%in={qn_%in_:5d} nblk~{-(-qn_//in_):3d} q_ok={qgot==qexp} r_ok={rgot==rexp}")
    else:
        print(f"  ok case {idx:2d} nb={nb_:5d} r={r_:.3f} na={na_:6d} qn={qn_:6d} in={in_:5d} qn%in={qn_%in_:5d}")

print(f"TOTAL {len(cases_data)} cases, FAILS={fails}")
