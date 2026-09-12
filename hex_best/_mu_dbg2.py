import subprocess, random, os, re

EXE = r"D:\precious_speed\hex_best\_cyc_test.exe"
PY = r"C:\Users\Administrator\.workbuddy\binaries\python\versions\3.13.12\python.exe"

random.seed(20260813)
nb = 200
na = max(3 * nb, int(nb * 3.0))
B = random.getrandbits(64 * nb)
if B < (1 << (64 * (nb - 1))):
    B |= (1 << (64 * (nb - 1)))
A = random.getrandbits(64 * na)
if A < (1 << (64 * (na - 1))):
    A |= (1 << (64 * (na - 1)))
if A < B:
    A += B

# --- reproduce C++ normalization ---
# sigma = clz(top limb of B)
top_limb = (B >> (64 * (nb - 1))) & 0xFFFFFFFFFFFFFFFF
# _lzcnt_u64
if top_limb == 0:
    sigma = 64
else:
    sigma = 0
    v = top_limb
    while not (v & (1 << 63)):
        v <<= 1
        sigma += 1
BS_val = B << sigma
# trim to nb limbs (should be exact)
BS_val &= ((1 << (64 * nb)) - 1)
assert BS_val.bit_length() <= 64 * nb

in_ = nb // 2  # 100
qn = na - nb   # 400
thi = in_      # 100
wstart = qn - thi  # 300
wlen = nb + thi    # 300

Z = A << sigma
# window = top wlen limbs of Z, starting at limb wstart
window = (Z >> (64 * wstart)) & ((1 << (64 * wlen)) - 1)
true_qhat1 = window // BS_val
true_qhat1_limbs = [(true_qhat1 >> (64 * i)) & 0xFFFFFFFFFFFFFFFF for i in range(thi + 1)]

# run binary
inp = "1\n" + format(A, 'X') + " " + format(B, 'X') + "\n"
env = {**os.environ, "MU": "1", "MUDBG": "1"}
r = subprocess.run([EXE], input=inp, capture_output=True, text=True, env=env)
print("=== STDOUT ===")
print(r.stdout.strip()[:200])
print("=== STDERR (MUDBG) ===")
print(r.stderr.strip())

# parse qhat_final
m = re.search(r"qhat_final\((\d+) limbs\):\s*([0-9a-f]+)", r.stderr)
if m:
    nlimb = int(m.group(1))
    hexs = m.group(2)
    # hex string is printed high limb first; reconstruct
    got = 0
    # m.group(2) is concatenation of %016llx from i=thi down to 0, so first 16 hex chars = top limb
    # length should be nlimb*16
    s = m.group(2)
    assert len(s) == nlimb * 16, (len(s), nlimb * 16)
    for i in range(nlimb):
        limb = int(s[i * 16:(i + 1) * 16], 16)
        got |= limb << (64 * (nlimb - 1 - i))
    got_limbs = [(got >> (64 * i)) & 0xFFFFFFFFFFFFFFFF for i in range(nlimb)]
    print("=== COMPARE block1 qhat (low limb first; idx0=LSB) ===")
    n = max(len(got_limbs), len(true_qhat1_limbs))
    bad = 0
    for i in range(n):
        g = got_limbs[i] if i < len(got_limbs) else 0
        t = true_qhat1_limbs[i] if i < len(true_qhat1_limbs) else 0
        flag = "" if g == t else "  <-- DIFF"
        if g != t:
            bad += 1
        print(f"  limb[{i:3d}] got={g:016x} true={t:016x}{flag}")
    print(f"DIFF limbs: {bad}")
    # also compare magnitude
    print(f"got  = {got}")
    print(f"true = {true_qhat1}")
    print(f"match exact: {got == true_qhat1}")
else:
    print("NO qhat_final parsed")
