import re, sys
sys.set_int_max_str_digits(800000)
BETA = 1 << 64

err = open(r'D:\precious_speed\tools\onecase_nd_err.txt').read()
def get(tag):
    m = re.search(tag + r'=([0-9A-Fa-f]+)', err)
    return int(m.group(1), 16)
V = get('V'); L = get('L'); QEST = get('QEST'); QE_FINAL = get('QE_FINAL')
out = open(r'D:\precious_speed\tools\onecase_nd_out.txt').read()
Qk = int(re.search(r'  Qk=([0-9A-Fa-f]+)', out).group(1), 16)
qn = int(re.search(r'qn=(\d+)', out).group(1), 16) if False else int(re.search(r'qn=(\d+)', out).group(1))

# derive an and n from L and V
an = L // V
bn = BETA ** qn   # hack: not used
# find n: v has n limbs; infer n from V's limb count via bit_length
n = (V.bit_length() + 63) // 64
print(f"derived: an bits={an.bit_length()} (expect ~{(qn)*64})  n={n}  V bits={V.bit_length()}")

b2n = BETA ** (2*n)
H = an // (BETA ** n)
M = L // b2n
F = an % (BETA ** n)
G = L % b2n
carry = 1 if (F * (BETA ** n) + G) >= b2n else 0
qest_recomputed = H + M + carry
print(f"H bits={H.bit_length()} M bits={M.bit_length()} carry={carry}")
print(f"QEST(dump) bits={QEST.bit_length()}")
print(f"qest_recomputed == QEST(dump)? {qest_recomputed == QEST}")
print(f"qest_recomputed == Qk(true)?   {qest_recomputed == Qk}")
print(f"QEST(dump)       == Qk(true)?   {QEST == Qk}")

# look at hex relationship
def hx(x, k=48):
    s = format(x, 'x')
    return (s[-k:] if len(s)>=k else s)
print("QEST(dump) low48:", hx(QEST))
print("qest_recomp low48:", hx(qest_recomputed))
print("Qk(true)   low48:", hx(Qk))
print("QEST(dump) high48:", hx(QEST, 48))
print("qest_recomp high48:", hx(qest_recomputed, 48))
print("Qk(true)   high48:", hx(Qk, 48))
