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

def limb_add(a, b):
    res = []
    carry = 0
    for i in range(max(len(a), len(b))):
        ai = a[i] if i < len(a) else 0
        bi = b[i] if i < len(b) else 0
        s = ai + bi + carry
        if s >= B:
            s -= B
            carry = 1
        else:
            carry = 0
        res.append(s)
    if carry:
        res.append(carry)
    return res, carry

def fft_ceil_cycm(n):
    m = 1
    while m < n:
        m <<= 1
    return m

def cyclic_mod(P, m):
    return int_to_limbs(P % (B ** m - 1), m)

def block(len2, this_in, divisor, qtrue_int, R_int, qhat_est, cyclic_m):
    d = limbs_to_int(divisor)
    window_int = qtrue_int + R_int * (B ** this_in)   # limb 拼接 = GMP rp
    rp = int_to_limbs(window_int, len2 + this_in)
    P = qhat_est * d
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
    # GMP r-based: 更新 rp (余数), r -= cy 收敛
    r = rp[len2] - tp[len2]
    rp_sub, cy = limb_sub(rp, tp)
    r -= cy
    cnt = 0
    while r != 0 and cnt < 10:
        if r > 0:
            qhat_est += 1
            rp, cy2 = limb_sub(rp, divisor)
        else:
            qhat_est -= 1
            rp, cy2 = limb_add(rp, divisor)
        r -= cy2
        cnt += 1
    return qhat_est == qtrue_int, cnt

def fuzz():
    fail = 0
    n = 20000
    fails = []
    for t in range(n):
        len2 = random.randint(10, 150)
        in_sz = random.randint(1, min(len2, 64))
        this_in = random.randint(1, in_sz)
        d_int = random.getrandbits(len2 * 16)
        if d_int <= 0:
            d_int = 1
        div_limbs = int_to_limbs(d_int, len2)
        div_limbs[len2 - 1] = max(div_limbs[len2 - 1], B // 2)
        divisor = div_limbs
        d = limbs_to_int(divisor)
        qtrue = random.getrandbits(this_in * 16) % (B ** this_in)
        R = random.randint(0, d - 1)
        cyclic_m = max(fft_ceil_cycm(len2 + 1), fft_ceil_cycm((len2 + in_sz) // 2 + 1))
        mode = random.choice(['none', 'limb+', 'limb-'])
        if mode == 'none':
            qhat_est = qtrue
        elif this_in > 1:
            qhat_est = qtrue + B ** (this_in - 1)
        else:
            qhat_est = qtrue + 1
        if mode == 'limb-' and this_in > 1:
            qhat_est = qtrue - B ** (this_in - 1)
        elif mode == 'limb-' and this_in == 1:
            qhat_est = qtrue - 1
        if qhat_est < 0:
            qhat_est = 0
        ok, cnt = block(len2, this_in, divisor, qtrue, R, qhat_est, cyclic_m)
        if not ok:
            fail += 1
            if len(fails) < 5:
                fails.append((len2, this_in, cyclic_m, mode, cnt))
    print(f"fuzz (GMP r-based + unwrap _err): {fail}/{n} failed")
    for f in fails:
        print("  FAIL:", f)

if __name__ == "__main__":
    fuzz()
