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

def limbs_to_int(limbs): return sum(a*(B**i) for i,a in enumerate(limbs))

def trace(divisor, dividend_limbs):
    div = list(dividend_limbs)
    N = len(div); len2 = len(divisor); divisor_high = divisor[len2-1]; dh_magic = ((1 << 48)//divisor_high) + 1
    divisor_high2 = divisor[len2-2] if len2 >= 2 else 0
    qlen = N - len2 + 1; quotient = [0]*qlen; quot_idx = N - len2
    Dint = limbs_to_int(divisor)
    # exact running remainder (Python big int) = div_high - (digits placed) * D
    div_high = limbs_to_int(dividend_limbs)
    # digits placed so far (above quot_idx) contribute: they are at positions > quot_idx
    placed = 0  # running: remainder = div_high - Q_placed * D, but we track per-iteration
    while quot_idx > 0:
        quot_idx -= 1
        len1 = quot_idx + len2
        high1 = div[len1]; high2 = div[len1-1]; high = high1*B + high2
        if high1 >= divisor_high: qh = B-1
        else: qh = (dh_magic*high) >> 48
        rhat = high - qh*divisor_high
        if len2 >= 2:
            u2 = div[len1-2]
            while rhat < B and qh*divisor_high2 > rhat*B + u2:
                qh -= 1; rhat += divisor_high
        # TRUE digit for this position = floor( (current_remainder_top) / D )? We instead verify via invariant later.
        true_qh = (high // divisor_high) if high1 < divisor_high else B-1
        qhat = qh
        bf = abs_sub_mul1(div, quot_idx, divisor, qhat)
        corr = 0
        while bf:
            qhat -= 1; corr += 1; bf = not abs_add(div, quot_idx, divisor)
        if qhat != true_qh:
            print(f"  ITER quot_idx={quot_idx}: high1={high1} high2={high2} high={high} div_high={divisor_high}")
            print(f"    qh_GM(or B-1)={qh} true_qh(high//dh)={true_qh} qhat_final={qhat} corrections={corr} rhat={rhat}")
            print(f"    NOTE: qhat_final != true_qh  -> BUG at this iteration")
            return False
        quotient[quot_idx] = qhat
    return True

random.seed(12345)
N = 48
for t in range(200):
    top = random.randint(HALF, B-1)
    d = [random.randint(0, B-1) for _ in range(N-1)] + [top]
    div = [0]*(2*N+1); div[2*N] = 1
    Dint = limbs_to_int(d); div_high = B**(2*N)
    ok = trace(d, div)
    if not ok:
        print(f"FAILED at t={t}, top={top}")
        # verify final
        q = [0]*(len(div)-len(d)+1)
        # recompute quotient via plain model just to show
        break
