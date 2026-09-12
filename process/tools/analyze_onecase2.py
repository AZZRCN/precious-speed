import re, sys
sys.set_int_max_str_digits(800000)
BETA = 1 << 64

# get a,b from the one-case input
lines = open(r'D:\precious_speed\tools\onecase_nd.txt').read().split('\n')
# format: "1", then "a b"
ab = [l.strip() for l in lines if l.strip()][1]  # skip the '1'
a_hex, b_hex = ab.split()
A = int(a_hex, 16); B = int(b_hex, 16)

def to_limbs(x):
    out=[]
    while x:
        out.append(x & ((1<<64)-1)); x >>= 64
    return out or [0]

na = len(to_limbs(A)); nb = len(to_limbs(B))
n = nb
# normalize
sigma = 0
if B > 0:
    # top limb
    btop = B >> (64*(nb-1))
    sigma = (64 - btop.bit_length()) % 64  # lzcnt
dn = B << sigma
an = A << sigma
# ensure an has na_an = na+1 limbs (top carry)
na_an = na + 1
an_mod = an  # as integer; floor(an/beta^n) etc use integer ops

# v from dump
err = open(r'D:\precious_speed\tools\onecase_nd_err.txt').read()
def get(tag):
    m = re.search(tag + r'=([0-9A-Fa-f]+)', err)
    return int(m.group(1), 16)
V = get('V'); L = get('L'); QEST = get('QEST')

# true q from stdout
out = open(r'D:\precious_speed\tools\onecase_nd_out.txt').read()
Qk = int(re.search(r'  Qk=([0-9A-Fa-f]+)', out).group(1), 16)

bn = BETA ** n
b2n = BETA ** (2*n)

# v_true = floor(b2n/dn) - bn
v_true = (b2n // dn) - bn
print(f"v (dump) bits={V.bit_length()}, v_true bits={v_true.bit_length()}, v<=v_true? {V <= v_true}, diff={abs(V-v_true).bit_length()}")

# L_true = an * v
L_true = an * V
print(f"L (dump) bits={L.bit_length()}, L_true bits={L_true.bit_length()}, L==L_true? {L==L_true}")

# H = floor(an/beta^n)
H = an // bn
# M = floor(L/beta^{2n}) = floor(an*v / beta^{2n})
M = L_true // b2n
# q_est_formula = H + M + modcarry
# modcarry: (an mod beta^n)*beta^n + (L mod beta^{2n}) >= beta^{2n} ?
F = an % bn           # an mod beta^n
G = L_true % b2n      # L mod beta^{2n}
carry = 1 if (F * bn + G) >= b2n else 0
qest_formula = H + M + carry
print(f"H bits={H.bit_length()}, M bits={M.bit_length()}, carry={carry}")
print(f"Qk(true) bits={Qk.bit_length()}")
print(f"QEST(dump) bits={QEST.bit_length()}")
print(f"qest_formula bits={qest_formula.bit_length()}")
print(f"qest_formula == Qk(true)? {qest_formula == Qk}")
print(f"QEST(dump) == Qk(true)? {QEST == Qk}")
print(f"qest_formula == QEST(dump)? {qest_formula == QEST}")
if qest_formula != Qk:
    d = qest_formula - Qk
    print(f"  qest_formula - Qk = {d}  (bits {abs(d).bit_length()})")
if QEST != qest_formula:
    d = QEST - qest_formula
    print(f"  QEST - qest_formula = {d}  (bits {abs(d).bit_length()})")
