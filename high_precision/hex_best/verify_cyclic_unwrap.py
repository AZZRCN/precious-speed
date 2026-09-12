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
    """a - b, len(a)>=len(b). 返回 (res, borrow)."""
    res = []
    borrow = 0
    for i in range(len(a)):
        ai = a[i] - borrow
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
    """P mod (B^m - 1), 半归一化 m-limb 表示 (limb < B)."""
    return int_to_limbs(P % (B ** m - 1), m)

def block(len2, this_in, divisor, qtrue_int, R_int, qhat_est, cyclic_m):
    """
    精确重建 HEX absDivMu 单块 (cyclic 路径):
      window = [qtrue(低 this_in limb), R(高 len2 limb)] 拼接
      P = qhat_est * divisor
      C = P mod (B^cyclic_m - 1)  (循环卷积)
      unwrap: tp[0..wn-1] -= rp[高 wn 位]; borrow 传播; _err 修正
      r-based: r = rp[len2] - tp[len2]; qhat 整数 ±1 收敛
    返回 (qhat_est==qtrue_int, corr_cnt)
    """
    d = limbs_to_int(divisor)
    window_int = qtrue_int + R_int * (B ** this_in)   # limb 拼接整数
    rp = int_to_limbs(window_int, len2 + this_in)
    P = qhat_est * d
    C = cyclic_mod(P, cyclic_m)
    tp = C[:]
    wn = len2 + this_in - cyclic_m
    if wn > 0:
        A = rp[cyclic_m: len2 + this_in]            # rp 高 wn 位
        tp_low = tp[:wn]
        tp_rest = tp[wn:]
        sub, borrow = limb_sub(tp_low, A)
        tp = sub + tp_rest
        i = wn
        while borrow and i < cyclic_m:
            tp[i] -= 1
            if tp[i] < 0:
                tp[i] += B
                borrow = 1
            else:
                borrow = 0
            i += 1
        # HEX D14 _err 修正 (L5996-6085)
        P_lo = P % (B ** cyclic_m)
        plo = int_to_limbs(P_lo, cyclic_m)
        tmod = tp[0] + tp[1] * B
        pmod = plo[0] + plo[1] * B
        err = (tmod + B2 - pmod) % B2
        if err > B2 // 2:
            err -= B2
        tp_int = limbs_to_int(tp) - err
        tp = int_to_limbs(tp_int % (B ** cyclic_m), cyclic_m)
    # r-based 修正
    r = rp[len2] - tp[len2]
    cnt = 0
    while r != 0 and cnt < 10:
        if r > 0:
            qhat_est += 1
        else:
            qhat_est -= 1
        P = qhat_est * d
        tp = int_to_limbs(P % (B ** cyclic_m), cyclic_m)
        r = rp[len2] - tp[len2]
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
        div_limbs[len2 - 1] = max(div_limbs[len2 - 1], B // 2)  # 归一化
        divisor = div_limbs
        d = limbs_to_int(divisor)
        qtrue = random.getrandbits(this_in * 16) % (B ** this_in)
        R = random.randint(0, d - 1)
        cyclic_m = max(fft_ceil_cycm(len2 + 1), fft_ceil_cycm((len2 + in_sz) // 2 + 1))
        bias = random.choice([0, 1, -1])
        qhat_est = qtrue + bias
        if qhat_est < 0:
            qhat_est = 0
        ok, cnt = block(len2, this_in, divisor, qtrue, R, qhat_est, cyclic_m)
        if not ok:
            fail += 1
            if len(fails) < 5:
                fails.append((len2, this_in, cyclic_m, bias, cnt))
    print(f"fuzz single-block (cyclic unwrap + r-based): {fail}/{n} failed")
    for f in fails:
        print("  FAIL case:", f)

if __name__ == "__main__":
    fuzz()
