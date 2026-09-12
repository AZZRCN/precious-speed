import random
B = 65536
B2 = B * B

def limbs_to_int(L):
    return sum(v * (B ** i) for i, v in enumerate(L)) if L else 0

def int_to_limbs(x, n):
    x %= (B ** n)
    L = []
    for _ in range(n):
        L.append(x % B)
        x //= B
    return L

def fft_ceil_cycm(n):
    m = 1
    while m < n:
        m <<= 1
    return m

# out[olo:olo+n] = a[alo:alo+n] - b[blo:blo+n] - borrow ; returns borrow
# matches GMP/C++ absSub(a, b, out): out = a - b (handles out aliasing a or b)
def absSub_at(a, alo, b, blo, out, olo, n):
    borrow = 0
    for i in range(n):
        ai = a[alo + i] if (alo + i) < len(a) else 0
        bi = b[blo + i] if (blo + i) < len(b) else 0
        s = ai - bi - borrow
        if s < 0:
            s += B
            borrow = 1
        else:
            borrow = 0
        out[olo + i] = s
    return borrow

# out[lo:hi] -= 1 ; returns 1 if underflow (shouldn't for our use)
def sub1_at(out, lo, hi):
    for i in range(lo, hi):
        if out[i] > 0:
            out[i] -= 1
            return 0
        else:
            out[i] = B - 1
    return 1

# out[lo:hi] += 1 ; returns 1 if overflow
def add1_at(out, lo, hi):
    for i in range(lo, hi):
        if out[i] + 1 < B:
            out[i] += 1
            return 0
        else:
            out[i] = 0
    return 1

# out[lo:hi] += a[alo:alo+L] ; returns carry
def add_limbs_at(out, lo, hi, a, alo):
    carry = 0
    L = hi - lo
    for i in range(L):
        oi = out[lo + i]
        ai = a[alo + i] if (alo + i) < len(a) else 0
        s = oi + ai + carry
        if s >= B:
            s -= B
            carry = 1
        else:
            carry = 0
        out[lo + i] = s
    return carry

# out[lo:hi] -= a[alo:alo+L] ; returns borrow
def sub_limbs_at(out, lo, hi, a, alo):
    borrow = 0
    L = hi - lo
    for i in range(L):
        oi = out[lo + i]
        ai = a[alo + i] if (alo + i) < len(a) else 0
        s = oi - ai - borrow
        if s < 0:
            s += B
            borrow = 1
        else:
            borrow = 0
        out[lo + i] = s
    return borrow

# compare a[alo:alo+n] vs b[blo:blo+n]
def cmp_at(a, alo, b, blo, n):
    for i in range(n - 1, -1, -1):
        ai = a[alo + i] if (alo + i) < len(a) else 0
        bi = b[blo + i] if (blo + i) < len(b) else 0
        if ai != bi:
            return 1 if ai > bi else -1
    return 0

def unwrap(len2, this_in, divisor, qhat_int, cyclic_m, window):
    P = qhat_int * limbs_to_int(divisor)
    C = P % (B ** cyclic_m - 1)
    tp = int_to_limbs(C, cyclic_m)
    wn = len2 + this_in - cyclic_m
    if wn > 0:
        # GMP L226: A = window[cyclic_m : len2+this_in] (high wn limbs)
        borrow = absSub_at(tp, 0, window, cyclic_m, tp, 0, wn)
        if borrow:
            sub1_at(tp, wn, cyclic_m)
        _q0 = qhat_int & (B - 1)
        _q1 = (qhat_int >> 16) & (B - 1)
        _d0 = divisor[0]
        _d1 = divisor[1] if len(divisor) > 1 else 0
        _pmod = (_q0 * _d0 + (_q0 * _d1 + _q1 * _d0) * B) % B2
        _tmod = (tp[0] + tp[1] * B) % B2
        _err = (_tmod + B2 - _pmod) % B2
        if _err > B2 // 2:
            _err -= B2
        if _err != 0:
            tp = int_to_limbs(limbs_to_int(tp) - _err, cyclic_m)
    return tp

def r_based_cyclic(window, divisor, tprod, qhat, this_in, len2):
    # tprod length >= len2+1 ; qhat length this_in+1
    r = int(window[len2]) - int(tprod[len2])
    cy = absSub_at(window, 0, tprod, 0, tprod, 0, this_in)
    if len2 != this_in:
        cy2 = absSub_at(window, this_in, tprod, this_in, tprod, this_in, len2 - this_in)
        if cy:
            c3 = sub1_at(tprod, this_in, len2)
            cy2 = cy2 or c3
        cy = cy2
    r -= cy
    corr = 0
    while r != 0 and corr < 10:
        if r > 0:
            add1_at(qhat, 0, this_in + 1)
            b = sub_limbs_at(tprod, 0, len2, divisor, 0)
            r -= b
        else:
            sub1_at(qhat, 0, this_in + 1)
            c = add_limbs_at(tprod, 0, len2, divisor, 0)
            r += c
        corr += 1
    if cmp_at(tprod, 0, divisor, 0, len2) >= 0:
        sub_limbs_at(tprod, 0, len2, divisor, 0)
        add1_at(qhat, 0, this_in + 1)
    return qhat, tprod[0:len2], corr, r

def cyclic_div(dividend_limbs, divisor_limbs, in_sz, DBG=False):
    len1 = len(dividend_limbs)
    len2 = len(divisor_limbs)
    qn = len1 - len2
    if qn <= 0:
        return None
    dividend = dividend_limbs[:]
    quotient = [0] * qn
    qn_remaining = qn
    inv_extra = 2 if (in_sz + 2 <= len2) else 0
    FORCE_EXTRA = True  # EXPERIMENT
    if FORCE_EXTRA:
        inv_extra = min(2, len2 - in_sz)
    dh_full = divisor_limbs[len2 - (in_sz + inv_extra):] if inv_extra else divisor_limbs[len2 - in_sz:]
    dh_int = limbs_to_int(dh_full)
    inv_full_int = (B ** (2 * (in_sz + inv_extra))) // dh_int
    inv_span_int = inv_full_int >> (inv_extra * 16)
    blk = 0
    while qn_remaining > 0:
        this_in = min(in_sz, qn_remaining)
        window = dividend[qn_remaining - this_in: qn_remaining + len2]
        win_int = limbs_to_int(window)
        d_int = limbs_to_int(divisor_limbs)
        true_qb = win_int // d_int
        divid_high_int = limbs_to_int(window[len2 - 1: len2 + this_in])
        qhat_full_int = divid_high_int * inv_span_int
        qhat_int = (qhat_full_int >> ((in_sz + 1) * 16)) & ((B ** (this_in + 1)) - 1)
        cyclic_m = max(fft_ceil_cycm(len2 + 1), fft_ceil_cycm((len2 + in_sz) // 2 + 1))
        use_cyclic = (cyclic_m < in_sz + len2)
        if use_cyclic:
            tprod = unwrap(len2, this_in, divisor_limbs, qhat_int, cyclic_m, window)
            qhat_arr = int_to_limbs(qhat_int, this_in + 1)
            qhat_arr, rem, corr, r = r_based_cyclic(window, divisor_limbs, tprod, qhat_arr, this_in, len2)
            cyc_qb = limbs_to_int(qhat_arr)
            if DBG and abs(cyc_qb - true_qb) > 0:
                print(f"  FAIL block{blk}: len2={len2} this_in={this_in} in={in_sz} cyc_m={cyclic_m} wn={len2+this_in-cyclic_m} use_cyc={use_cyclic}")
                print(f"    qhat_pre={qhat_int} true_qb={true_qb} cyc_qb={cyc_qb} diff={cyc_qb-true_qb} r={r} corr={corr}")
                return None
        else:
            qb = win_int // d_int
            qhat_arr = int_to_limbs(qb, this_in + 1)
            rem_int = win_int - qb * d_int
            rem = int_to_limbs(rem_int, len2)
        for i in range(this_in):
            quotient[qn_remaining - this_in + i] = qhat_arr[i]
        for i in range(len2):
            dividend[qn_remaining - this_in + i] = rem[i] if i < len(rem) else 0
        qn_remaining -= this_in
        blk += 1
    return quotient

def fuzz(n=3000, seed=0):
    random.seed(seed)
    fail = 0
    ex = []
    for t in range(n):
        len2 = random.randint(8, 300)
        qn = random.randint(1, 200)
        len1 = len2 + qn
        in_sz = random.randint(max(1, len2 // 4), len2 // 2)
        a_int = random.getrandbits(len1 * 16)
        if a_int < (B ** len2):
            a_int += B ** len2
        b_int = random.getrandbits(len2 * 16)
        if b_int < (B ** (len2 - 1)):
            b_int += B ** (len2 - 1)
        a = int_to_limbs(a_int, len1)
        b = int_to_limbs(b_int, len2)
        q_cyclic = cyclic_div(a, b, in_sz)
        if q_cyclic is None:
            continue
        q_true = int_to_limbs(a_int // b_int, qn)
        if q_cyclic != q_true:
            fail += 1
            if len(ex) < 5:
                ex.append((len2, qn, in_sz))
    print(f"cyclic-div logic fuzz (seed={seed}): fail={fail}/{n}")
    for e in ex:
        print("  eg:", e)

if __name__ == "__main__":
    fuzz()
