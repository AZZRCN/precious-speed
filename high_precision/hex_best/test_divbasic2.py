import sys
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
    i = n + 1
    while i < (a_start + n + 1) and borrow:
        r, borrow = sub_half(dividend[i], 1, B); dividend[i] = r; i += 1
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
    if divisor_high < HALF:
        return None  # need normalization; skip
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
def int_to_limbs(v, n):
    out = []
    for i in range(n): out.append(v % B); v //= B
    return out

def check(name, dividend_init, divisor):
    if divisor[-1] < HALF:
        print(f"[{name}] SKIP (divisor not normalized, top={divisor[-1]})"); return
    div = list(dividend_init)
    q = abs_div_basic_core(div, divisor)
    Dint = limbs_to_int(divisor); Qint = limbs_to_int(q); Rint = limbs_to_int(div[:len(divisor)])
    div_high = limbs_to_int(dividend_init)
    trueQ = div_high // Dint; trueR = div_high - trueQ*Dint
    okQ = (Qint == trueQ)
    okInv = (Qint*Dint + Rint == div_high)
    okRem = (Rint < Dint)
    okR = (Rint == trueR)
    print(f"[{name}] Q==true? {okQ} invOK? {okInv} R<D? {okRem} R==trueRem? {okR}  Q[0]={q[0]} trueQ[0]={trueQ % B}")
    if not okQ:
        for i in range(min(len(q), 200)):
            if q[i] != (trueQ >> (i*16)) % B:
                print(f"    first diff limb {i}: model={q[i]} true={(trueQ>>(i*16))%B}")
                break

# normalized 2-limb
check("D=[0,32768]", [0,0,1], [0,32768])
check("D=[1,32768]", [0,0,1], [1,32768])
check("D=[40000,50000]", [0,0,1], [40000,50000])
check("D=[40000,50000] B^4", [0,0,0,0,1], [40000,50000])
check("D=[100,200,50000]", [0,0,1], [100,200,50000])
check("D=[123,456,50000] B^6", [0,0,0,0,0,0,1], [123,456,50000])
# 4-limb
check("D=[1,2,3,50000] B^8", [0]*8+[1], [1,2,3,50000])
# 5-limb
check("D=[9,8,7,6,50000] B^10", [0]*10+[1], [9,8,7,6,50000])
