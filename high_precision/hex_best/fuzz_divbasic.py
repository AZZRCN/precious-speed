import random
B = 65536
HALF = B//2

def sub_half(a, b, base):
    if a < b: return a - b + base, True
    return a - b, False
def add_half(a, b, base):
    r = a + b
    if r >= base: return r - base, True
    return r, False
def abs_sub_mul1(dividend, a_start, in_limbs, x):
    n = len(in_limbs); carry_q = 0; borrow = False; i = 0
    while i < n:
        prod = in_limbs[i]*x + carry_q; m = prod % B; carry_q = prod // B
        b = (m + (1 if borrow else 0)) % B
        r, borrow = sub_half(dividend[a_start+i], b, B); dividend[a_start+i] = r; i += 1
    b = (carry_q + (1 if borrow else 0)) % B
    r, borrow = sub_half(dividend[a_start+n], b, B); dividend[a_start+n] = r
    return borrow
def abs_add(dividend, a_start, in_limbs):
    carry = False; n = len(in_limbs)
    for i in range(n):
        r, carry = add_half(dividend[a_start+i], in_limbs[i] + (1 if carry else 0), B); dividend[a_start+i] = r
    if carry:
        r, carry = add_half(dividend[a_start+n], 1, B); dividend[a_start+n] = r
    return carry
def abs_div_basic_core(dividend, divisor):
    len1 = len(dividend); len2 = len(divisor); divisor_high = divisor[len2-1]
    dh_magic = ((1 << 48)//divisor_high) + 1
    divisor_high2 = divisor[len2-2] if len2 >= 2 else 0
    qlen = len1 - len2 + 1; quotient = [0]*qlen
    quot_idx = len1 - len2
    while quot_idx > 0:
        quot_idx -= 1
        len1 = quot_idx + len2
        high1 = dividend[len1]; high2 = dividend[len1-1]; high = high1*B + high2
        if high1 >= divisor_high: qh = B-1
        else: qh = (dh_magic*high) >> 48
        rhat = high - qh*divisor_high
        if len2 >= 2:
            u2 = dividend[len1-2]
            while rhat < B and qh*divisor_high2 > rhat*B + u2:
                qh -= 1; rhat += divisor_high
        qhat = qh
        bf = abs_sub_mul1(dividend, quot_idx, divisor, qhat)
        while bf:
            qhat -= 1; bf = not abs_add(dividend, quot_idx, divisor)
        quotient[quot_idx] = qhat
    return quotient

def limbs_to_int(limbs): return sum(a*(B**i) for i,a in enumerate(limbs))

def trial(divisor, dividend_limbs):
    div = list(dividend_limbs)
    q = abs_div_basic_core(div, divisor)
    Dint = limbs_to_int(divisor); Qint = limbs_to_int(q); Rint = limbs_to_int(div[:len(divisor)])
    div_high = limbs_to_int(dividend_limbs)
    trueQ = div_high // Dint; trueR = div_high - trueQ*Dint
    invOK = (Qint*Dint + Rint == div_high)
    remOK = (Rint < Dint)
    qOK = (Qint == trueQ)
    return qOK, invOK, remOK, (Rint == trueR)

random.seed(12345)
N = 48
fails = 0
for t in range(200):
    top = random.randint(HALF, B-1)
    d = [random.randint(0, B-1) for _ in range(N-1)] + [top]
    div = [0]*(2*N+1); div[2*N] = 1  # B^(2N)
    qOK, invOK, remOK, rOK = trial(d, div)
    if not (qOK and invOK and remOK and rOK):
        fails += 1
        if fails <= 3:
            print(f"  FAIL t={t}: qOK={qOK} invOK={invOK} remOK={remOK} rOK={rOK} top={top}")
print(f"random 48-limb: {fails}/200 failed")

# also try with low limb even specifically (like real D[0]=18468 even)
fails2 = 0
for t in range(200):
    top = random.randint(HALF, B-1)
    d = [random.choice([0,2,4,6,8])*1 + random.randint(0, B//2-1)*2 for _ in range(N-1)] + [top]  # ensure even low limb
    d[0] = random.choice([2,4,6,8,10,100,1000,18468])  # even low limb
    div = [0]*(2*N+1); div[2*N] = 1
    qOK, invOK, remOK, rOK = trial(d, div)
    if not (qOK and invOK and remOK and rOK):
        fails2 += 1
        if fails2 <= 3:
            print(f"  EVEN-FAIL t={t}: qOK={qOK} invOK={invOK} remOK={remOK} rOK={rOK} top={top} d0={d[0]}")
print(f"random 48-limb even-low: {fails2}/200 failed")
