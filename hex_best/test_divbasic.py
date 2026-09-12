import sys
B = 65536

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
def abs_div_basic_core(dividend, divisor, trace=False):
    len1 = len(dividend); len2 = len(divisor); divisor_high = divisor[len2-1]
    assert divisor_high >= B//2
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

def check(name, dividend_init, divisor, expected_Q_limbs):
    div = list(dividend_init)
    q = abs_div_basic_core(div, divisor)
    Dint = limbs_to_int(divisor)
    Qint = limbs_to_int(q)
    Rint = limbs_to_int(div[:len(divisor)])
    div_high = limbs_to_int(dividend_init)
    okQ = (q == expected_Q_limbs)
    okInv = (Qint*Dint + Rint == div_high)
    okRem = (Rint < Dint)
    print(f"[{name}] Q==true? {okQ}  invariant(Q*D+R==div)? {okInv}  R<D? {okRem}  Q[0]={q[0]}")
    if not okQ:
        for i in range(min(len(q), len(expected_Q_limbs))):
            if q[i] != expected_Q_limbs[i]:
                print(f"    first diff limb {i}: model={q[i]} true={expected_Q_limbs[i]}")
                break
    return okQ, okInv, okRem

# Case 1: trivial single-limb divisor
D1 = [2]
div1 = [0,0,1]  # B^2
true1 = int_to_limbs((B*B)//2, 3)
check("single-limb D=2", div1, D1, true1)

# Case 2: single-limb max
D2 = [65535]
div2 = [0,0,1]
true2 = int_to_limbs((B*B)//65535, 3)
check("single-limb D=65535", div2, D2, true2)

# Case 3: two-limb
D3 = [1,2]  # 2*B+1 = 131073
div3 = [0,0,0,0,1]  # B^4
true3 = int_to_limbs((B**4)//(2*B+1), 5)
check("two-limb D=[1,2]", div3, D3, true3)

# Case 4: two-limb, normalized top already >= B/2
D4 = [40000, 50000]
div4 = [0,0,0,0,1]
true4 = int_to_limbs((B**4)//limbs_to_int(D4), 5)
check("two-limb D=[40000,50000]", div4, D4, true4)

# Case 5: three-limb
D5 = [100, 200, 300]
div5 = [0]*8; div5[8] = 1  # B^8
true5 = int_to_limbs((B**8)//limbs_to_int(D5), 9)
check("three-limb D=[100,200,300]", div5, D5, true5)
