import sys, re

B = 65536

def parse_block(path):
    blocks = []
    cur = {}
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("K="):
                if cur: blocks.append(cur)
                m = re.match(r"K=(\d+)\s+S=(\d+)\s+INV0LEN=(\d+)\s+PRODSIZE=(\d+)", line)
                cur = {"K":int(m.group(1)),"S":int(m.group(2)),
                       "INV0LEN":int(m.group(3)),"PRODSIZE":int(m.group(4))}
            elif "=" in line and line.split("=")[0] in ("M","INV0","REALINV0","SQRT","PROD","INV","REF"):
                key, val = line.split("=",1)
                nums = [int(x) for x in val.split(",") if x != ""]
                cur[key] = nums
    if cur: blocks.append(cur)
    return blocks

def limbs_to_int(limbs):
    return sum(a * (B**i) for i,a in enumerate(limbs))

def int_to_limbs(v, n):
    out = []
    for i in range(n):
        out.append(v % B); v //= B
    return out

def sub_half(a, b, base):
    if a < b:
        return a - b + base, True
    return a - b, False

def add_half(a, b, base):
    r = a + b
    if r >= base:
        return r - base, True
    return r, False

def abs_sub_mul1(dividend, a_start, in_limbs, x):
    n = len(in_limbs)
    carry_q = 0
    borrow = False
    i = 0
    while i < n:
        prod = in_limbs[i] * x + carry_q
        m = prod % B
        carry_q = prod // B
        b = (m + (1 if borrow else 0)) % B
        r, borrow = sub_half(dividend[a_start + i], b, B)
        dividend[a_start + i] = r
        i += 1
    # top limb
    b = (carry_q + (1 if borrow else 0)) % B
    r, borrow = sub_half(dividend[a_start + n], b, B)
    dividend[a_start + n] = r
    # NOTE: C++ span has size n+1, so 'for(; i<acc.size && bw; i++)' with i==n+1
    # never executes. Borrow is returned and handled by the caller's while(bf).
    return borrow

def abs_add(dividend, a_start, in_limbs):
    carry = False
    n = len(in_limbs)
    for i in range(n):
        r, carry = add_half(dividend[a_start + i], in_limbs[i] + (1 if carry else 0), B)
        dividend[a_start + i] = r
    if carry:
        r, carry = add_half(dividend[a_start + n], 1, B)
        dividend[a_start + n] = r
    return carry

def abs_div_basic_core(dividend, divisor):
    # dividend: mutable list (limbs). divisor: list. returns quotient list.
    len1 = len(dividend)
    len2 = len(divisor)
    divisor_high = divisor[len2-1]
    assert divisor_high >= B//2
    dh_magic = ((1 << 48) // divisor_high) + 1
    divisor_high2 = divisor[len2-2] if len2 >= 2 else 0
    qlen = len1 - len2 + 1
    quotient = [0]*qlen
    quot_idx = len1 - len2
    while quot_idx > 0:
        quot_idx -= 1
        len1 = quot_idx + len2
        high1 = dividend[len1]
        high2 = dividend[len1-1]
        high = high1*B + high2
        if high1 >= divisor_high:
            qh = B - 1
        else:
            qh = high // divisor_high   # EXACT (replacing GM) to isolate bug
        rhat = high - qh * divisor_high
        if len2 >= 2:
            u2 = dividend[len1-2]
            while rhat < B and qh * divisor_high2 > rhat * B + u2:
                qh -= 1
                rhat += divisor_high
        qhat = qh
        bf = abs_sub_mul1(dividend, quot_idx, divisor, qhat)
        while bf:
            qhat -= 1
            bf = not abs_add(dividend, quot_idx, divisor)
        quotient[quot_idx] = qhat
        # dividend "size" reduced; not needed in model
    return quotient

path = sys.argv[1]
blocks = parse_block(path)
blk = blocks[0]
K = blk["K"]; S = blk["S"]; M = blk["M"]; REALINV0 = blk["REALINV0"]; REF = blk["REF"]

# === BASE CASE (k'=48): divisor = M[47:], dividend = B^96 (97 limbs) ===
D = M[S:]               # 48 limbs
div = [0]*97; div[96] = 1
q_model = abs_div_basic_core(div, D)
true_inv0 = limbs_to_int(D) // (B**96)
true_inv0_limbs = int_to_limbs(true_inv0, 49)
print(f"[base] model quotient (49 limbs) == REALINV0? {q_model == REALINV0[:49]}")
print(f"[base] model quotient == TRUE floor(B^96/M[47:])? {q_model == true_inv0_limbs}")
if q_model != true_inv0_limbs:
    for i in range(49):
        if q_model[i] != true_inv0_limbs[i]:
            print(f"    first DIFF at limb {i}: model={q_model[i]} true={true_inv0_limbs[i]} REALINV0={REALINV0[i]}")
            break
print(f"[base] REALINV0[0]={REALINV0[0]} model[0]={q_model[0]} true[0]={true_inv0_limbs[0]}")

# === REF CASE (k=95): divisor = M (95 limbs), dividend = B^190 (191 limbs) ===
Mval_limbs = M
div2 = [0]*(2*K+1); div2[2*K] = 1
q_model2 = abs_div_basic_core(div2, Mval_limbs)
true_ref = limbs_to_int(M) // (B**(2*K))
true_ref_limbs = int_to_limbs(true_ref, K+1)
print(f"\n[ref]  model quotient (96 limbs) == C++ REF? {q_model2 == REF[:K+1]}")
print(f"[ref]  model quotient == TRUE floor(B^{2*K}/M)? {q_model2 == true_ref_limbs}")
if q_model2 != true_ref_limbs:
    for i in range(K+1):
        if q_model2[i] != true_ref_limbs[i]:
            print(f"    first DIFF at limb {i}: model={q_model2[i]} true={true_ref_limbs[i]} REF={REF[i]}")
            break
