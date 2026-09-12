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

def limb_sub(a, b):
    res = []
    borrow = 0
    for i in range(max(len(a), len(b))):
        ai = (a[i] - borrow) if i < len(a) else -borrow
        bi = b[i] if i < len(b) else 0
        s = ai - bi
        if s < 0:
            s += B
            borrow = 1
        else:
            borrow = 0
        res.append(s)
    return res, borrow

def fft_ceil_cycm(n):
    m = 1
    while m < n:
        m <<= 1
    return m

def cyclic_mod(P, m):
    return int_to_limbs(P % (B ** m - 1), m)

def limbs_less(a, b):
    n = max(len(a), len(b))
    a = a + [0] * (n - len(a))
    b = b + [0] * (n - len(b))
    for i in range(len(a) - 1, -1, -1):
        if a[i] != b[i]:
            return a[i] < b[i]
    return False

def unwrap_hexc(len2, this_in, divisor, qhat, R, cyclic_m):
    d = limbs_to_int(divisor)
    window_int = qhat + R * (B ** this_in)
    rp = int_to_limbs(window_int, len2 + this_in)
    P = qhat * d
    C = cyclic_mod(P, cyclic_m)
    tp = C[:]
    wn = len2 + this_in - cyclic_m
    if wn > 0:
        A = rp[cyclic_m: len2 + this_in]
        sub, borrow = limb_sub(tp[:wn], A)
        tp = sub + tp[wn:]
        i = wn
        while borrow and i < cyclic_m:
            tp[i] -= 1
            if tp[i] < 0:
                tp[i] += B
                borrow = 1
            else:
                borrow = 0
            i += 1
        P_lo = P % (B ** cyclic_m)
        plo = int_to_limbs(P_lo, cyclic_m)
        tmod = tp[0] + tp[1] * B
        pmod = plo[0] + plo[1] * B
        err = (tmod + B2 - pmod) % B2
        if err > B2 // 2:
            err -= B2
        tp = int_to_limbs(limbs_to_int(tp) - err, cyclic_m)
    return tp[len2]

def unwrap_gmp(len2, this_in, divisor, qhat, R, cyclic_m):
    d = limbs_to_int(divisor)
    window_int = qhat + R * (B ** this_in)
    rp = int_to_limbs(window_int, len2 + this_in)
    P = qhat * d
    C = cyclic_mod(P, cyclic_m)
    tp = C[:]
    wn = len2 + this_in - cyclic_m
    if wn > 0:
        A = rp[cyclic_m: len2 + this_in]
        sub, borrow = limb_sub(tp[:wn], A)
        tp = sub + tp[wn:]
        i = wn
        while borrow and i < cyclic_m:
            tp[i] -= 1
            if tp[i] < 0:
                tp[i] += B
                borrow = 1
            else:
                borrow = 0
            i += 1
        cy = borrow
        cmp_len = cyclic_m - len2
        cx = 0
        if cmp_len > 0 and (len2 - this_in) >= 0:
            rp_seg = rp[len2 - this_in: len2 - this_in + cmp_len]
            tp_seg = tp[len2: len2 + cmp_len]
            cx = 1 if limbs_less(rp_seg, tp_seg) else 0
        incr = cx - cy   # GMP 双向 (允许 cy > cx)
        tp = int_to_limbs(limbs_to_int(tp) + incr, cyclic_m)
    return tp[len2]

def fuzz():
    n = 20000
    fail_h = fail_g = 0
    ex = []
    for t in range(n):
        len2 = random.randint(10, 200)
        in_sz = random.randint(1, min(len2, 64))
        this_in = random.randint(1, in_sz)
        d_int = random.getrandbits(len2 * 16)
        if d_int <= 0:
            d_int = 1
        div_limbs = int_to_limbs(d_int, len2)
        div_limbs[len2 - 1] = max(div_limbs[len2 - 1], B // 2)
        divisor = div_limbs
        d = limbs_to_int(divisor)
        qhat = random.getrandbits(this_in * 16) % (B ** this_in)
        R = random.randint(0, d - 1)
        cyclic_m = max(fft_ceil_cycm(len2 + 1), fft_ceil_cycm((len2 + in_sz) // 2 + 1))
        wn = len2 + this_in - cyclic_m
        if wn <= 0:
            continue   # 只测 unwrap 活跃区
        P = qhat * d
        P_len2 = int_to_limbs(P, len2 + this_in)[len2]
        th = unwrap_hexc(len2, this_in, divisor, qhat, R, cyclic_m)
        tg = unwrap_gmp(len2, this_in, divisor, qhat, R, cyclic_m)
        if th != P_len2:
            fail_h += 1
        if tg != P_len2:
            fail_g += 1
        if (th != P_len2 or tg != P_len2) and len(ex) < 6:
            ex.append((len2, this_in, cyclic_m, wn, qhat % 10, R % 10, th, tg, P_len2))
    print(f"unwrap-active cases: HEX _err wrong={fail_h}, GMP cx-cy wrong={fail_g}")
    for e in ex:
        print("  eg:", e)

if __name__ == "__main__":
    fuzz()
