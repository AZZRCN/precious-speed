import subprocess, random, os
EXE = r"D:\precious_speed\hex_best\_cyc_test.exe"
PY = r"C:\Users\Administrator\.workbuddy\binaries\python\versions\3.13.12\python.exe"

def gen_case(nb, na_ratio, rng):
    B = rng.getrandbits(64 * nb)
    if B < (1 << (64 * (nb - 1))):
        B |= (1 << (64 * (nb - 1)))
    na = max(3 * nb, int(nb * na_ratio))
    A = rng.getrandbits(64 * na)
    if A < (1 << (64 * (na - 1))):
        A |= (1 << (64 * (na - 1)))
    if A < B:
        A += B
    return A, B

rng = random.Random(20260813)
# 复现 case 0: nb=200, ratio 3.0
A, B = gen_case(200, 3.0, rng)
src = f"1\n{format(A,'X')} {format(B,'X')}\n"
env = {**os.environ, "MU":"1"}
r = subprocess.run([EXE], input=src, capture_output=True, text=True, env=env)
print("rc", r.returncode, "stderr", r.stderr[:500])
out = r.stdout.strip().split("\n")
parts = out[0].split()
qgot = int(parts[0],16); rgot = int(parts[1],16)
qexp = A//B; rexp = A%B
print("consistent(q*B+r==A)?", qgot*B+rgot == A)
print("qgot==qexp?", qgot==qexp, " rgot==rexp?", rgot==rexp)
print("qexp bits", qexp.bit_length(), "qgot bits", qgot.bit_length())
print("A==qgot*B+rgot?", A==qgot*B+rgot)
print("A==qexp*B+rexp?", A==qexp*B+rexp)
diff = qgot - qexp
print("qgot-qexp =", diff)
if diff != 0:
    print("diff in hex:", format(diff if diff>0 else -diff, 'X'), "sign", "+" if diff>0 else "-")
    print("diff % B == 0?", (diff % B)==0)
