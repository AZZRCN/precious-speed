import random
B = 65536

def sub_half(a, b, base):
    if a < b: return a - b + base, True
    return a - b, False
def add_half(a, b, base):
    r = a + b
    if r >= base: return r - base, True
    return r, False

# Model's absSubMul1 (faithful to C++ scalar path)
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

def limbs_to_int(limbs): return sum(a*(B**i) for i,a in enumerate(limbs))

random.seed(99)
fails_sub = 0
for t in range(5000):
    n = random.randint(1, 60)
    acc = [random.randint(0, B-1) for _ in range(n+1)]
    inl = [random.randint(0, B-1) for _ in range(n)]
    x = random.randint(0, B-1)
    acc_int = limbs_to_int(acc)
    in_int = limbs_to_int(inl)
    ref = (acc_int - x*in_int) % (B**(n+1))
    ref_limbs = [(ref >> (16*i)) % B for i in range(n+1)]
    div = list(acc)
    bw = abs_sub_mul1(div, 0, inl, x)
    if div != ref_limbs:
        fails_sub += 1
        if fails_sub <= 2:
            print(f"  SUBMUL FAIL t={t}: bw={bw} ref_bw={(acc_int < x*in_int)}")
            for i in range(n+1):
                if div[i] != ref_limbs[i]:
                    print(f"    limb {i}: model={div[i]} ref={ref_limbs[i]}")
                    break
print(f"absSubMul1: {fails_sub}/5000 failed")

fails_add = 0
for t in range(5000):
    n = random.randint(1, 60)
    acc = [random.randint(0, B-1) for _ in range(n+1)]
    inl = [random.randint(0, B-1) for _ in range(n)]
    acc_int = limbs_to_int(acc); in_int = limbs_to_int(inl)
    ref = (acc_int + in_int) % (B**(n+1))
    ref_limbs = [(ref >> (16*i)) % B for i in range(n+1)]
    div = list(acc)
    cf = abs_add(div, 0, inl)
    if div != ref_limbs:
        fails_add += 1
        if fails_add <= 2:
            print(f"  ADD FAIL t={t}: cf={cf}")
            for i in range(n+1):
                if div[i] != ref_limbs[i]:
                    print(f"    limb {i}: model={div[i]} ref={ref_limbs[i]}")
                    break
print(f"absAdd: {fails_add}/5000 failed")
