import re, sys
sys.set_int_max_str_digits(800000)
BETA = 1 << 64

lines = open(r'D:\precious_speed\tools\onecase_nd.txt').read().split('\n')
ab = [l.strip() for l in lines if l.strip()][1]
a_hex, b_hex = ab.split()
A = int(a_hex, 16); B = int(b_hex, 16)

def sig_limbs(x):
    if x == 0: return 1
    return max(1, (x.bit_length() + 63) // 64)

na = sig_limbs(A); nb = sig_limbs(B)
n = nb
# top limbs
atop = A >> (64*(na-1)); btop = B >> (64*(nb-1))
sigma = 64 - btop.bit_length()  # lzcnt of btop
dn = B << sigma
an = A << sigma
print(f"na={na} nb={nb} n={n} sigma={sigma}")
print(f"an//dn == A//B ? { (an//dn) == (A//B) }   A//B={A//B}")
true_q = A // B
dn_top_set = (dn >> (64*(n-1) + 63)) & 1
print(f"dn top bit set? {dn_top_set}  dn limbs={sig_limbs(dn)} (expect {n})")

b2n = BETA ** (2*n); bn = BETA ** n
v_true = (b2n // dn) - bn
err = open(r'D:\precious_speed\tools\onecase_nd_err.txt').read()
def get(tag):
    m = re.search(tag + r'=([0-9A-Fa-f]+)', err); return int(m.group(1),16) if m else None
V = get('V'); QEST = get('QEST'); QE_FINAL = get('QE_FINAL')
print(f"V bits={V.bit_length()} v_true bits={v_true.bit_length()}")
print(f"V <= v_true? {V <= v_true}   |V-v_true| bits={abs(V-v_true).bit_length()}")

# formula using TRUE v_true (ground truth reciprocal)
H = an // bn
Mtrue = (an * v_true) // b2n
F = an % bn; G = (an * v_true) % b2n
carry_t = 1 if (F*bn + G) >= b2n else 0
qest_true = H + Mtrue + carry_t
print(f"[using v_true] qest_true bits={qest_true.bit_length()} == true_q? {qest_true == true_q}")

# formula using dumped V
Mv = (an * V) // b2n
Fv = an % bn; Gv = (an * V) % b2n
carry_v = 1 if (Fv*bn + Gv) >= b2n else 0
qest_V = H + Mv + carry_v
print(f"[using dumped V] qest_V bits={qest_V.bit_length()} == true_q? {qest_V == true_q}")
print(f"qest_V == QEST(dump)? {qest_V == QEST}")

# direct: is an*v the same as the L dump?
L = get('L')
print(f"L == an*V? {L == an*V}")
print(f"an*v_true == ?  (an*v_true)//b2n vs Mtrue ok)")
