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
            elif line.strip() == "" and cur:
                pass
    if cur: blocks.append(cur)
    return blocks

def limbs_to_int(limbs):
    v = 0
    for i,a in enumerate(limbs):
        v += a * (B**i)
    return v

def int_to_limbs(v, n):
    out = []
    for i in range(n):
        out.append(v % B)
        v //= B
    return out

path = sys.argv[1]
blocks = parse_block(path)
print(f"parsed {len(blocks)} block(s)")
for blk in blocks:
    K = blk["K"]; S = blk["S"]; INV0LEN = blk["INV0LEN"]; PRODSIZE = blk["PRODSIZE"]
    M = blk["M"]; REALINV0 = blk["REALINV0"]; SQRT = blk["SQRT"]
    PROD = blk["PROD"]; INV = blk["INV"]; REF = blk["REF"]
    Mval = limbs_to_int(M)
    k = K
    ks = k - S            # = 48, size of recursion divisor
    D = M[S:]             # M[47:]  (48 limbs)
    Dval = limbs_to_int(D)
    # ---- STAGE 0: base case inv0 = floor(B^(2*ks)/D) ----
    true_inv0 = Dval // (B ** (2*ks))
    true_inv0_limbs = int_to_limbs(true_inv0, INV0LEN)
    ok0 = (true_inv0_limbs == REALINV0[:INV0LEN])
    print(f"\n=== K={K} S={S} INV0LEN={INV0LEN} PRODSIZE={PRODSIZE} ===")
    print(f"[0] REALINV0 == exact floor(B^{2*ks}/M[{S}:])? {ok0}")
    if not ok0:
        for i in range(INV0LEN):
            if true_inv0_limbs[i] != REALINV0[i]:
                print(f"    first diff inv0 at limb {i}: exact={true_inv0_limbs[i]} got={REALINV0[i]}")
                break
    # ---- STAGE 1: SQRT = inv0^2 ----
    inv0_int = limbs_to_int(REALINV0[:INV0LEN])   # use C++ inv0 (what absSqr actually squared)
    true_sq = inv0_int * inv0_int
    sq_limbs = int_to_limbs(true_sq, PRODSIZE)
    ok1 = (sq_limbs == SQRT[:PRODSIZE])
    print(f"[1] SQRT == (C++ inv0)^2 (limbs {PRODSIZE})? {ok1}")
    if not ok1:
        for i in range(PRODSIZE):
            if sq_limbs[i] != SQRT[i]:
                print(f"    first diff SQRT at limb {i}: exact={sq_limbs[i]} got={SQRT[i]}")
                break
    # ---- STAGE 2: PROD = inv0^2 * M ----
    true_prod = true_sq * Mval
    prod_limbs = int_to_limbs(true_prod, PRODSIZE)
    ok2 = (prod_limbs == PROD[:PRODSIZE])
    print(f"[2] PROD == (C++ inv0)^2 * M (limbs {PRODSIZE})? {ok2}")
    if not ok2:
        for i in range(PRODSIZE):
            if prod_limbs[i] != PROD[i]:
                print(f"    first diff PROD at limb {i}: exact={prod_limbs[i]} got={PROD[i]}")
                break
    # ---- STAGE 3: INV = 2*inv0 - high(inv0^2 * M)  where high = prod >> (2*ks) ----
    # C++: inv2_span = 2*inv0 (48-limb doubling stored at tinv2+s, i.e. shifted by s=47)
    # inv2_span[0..] = 2*inv0 shifted: low s limbs zero. So as integer = (2*inv0) * B^s
    inv2_int = (2 * inv0_int) * (B ** S)
    # high part = (inv0^2 * M) >> (2*ks)
    high_part = true_prod // (B ** (2*ks))
    inv_new = inv2_int - high_part
    inv_new_limbs = int_to_limbs(inv_new, k+1)
    ok3 = (inv_new_limbs == INV[:k+1])
    print(f"[3] INV == 2*inv0*B^s - high(inv0^2*M) (formula)? {ok3}")
    if not ok3:
        for i in range(k+1):
            if inv_new_limbs[i] != INV[i]:
                print(f"    first diff INV at limb {i}: formula={inv_new_limbs[i]} got={INV[i]}")
                break
    # ---- STAGE 4: REF = exact floor(B^(2k)/M) ----
    ref_val = Mval // (B ** (2*k))
    ref_limbs = int_to_limbs(ref_val, k+1)
    ok4 = (ref_limbs == REF[:k+1])
    print(f"[4] REF == exact floor(B^{2*k}/M)? {ok4}")
    if not ok4:
        for i in range(k+1):
            if ref_limbs[i] != REF[i]:
                print(f"    first diff REF at limb {i}: exact={ref_limbs[i]} got={REF[i]}")
                break
    # ---- STAGE 5: does the formula (with EXACT inv0) match REF? ----
    true_inv0_int = true_inv0
    inv2b = (2 * true_inv0_int) * (B ** S)
    sqb = true_inv0_int * true_inv0_int
    prodb = sqb * Mval
    highb = prodb // (B ** (2*ks))
    inv_new_b = inv2b - highb
    invb_limbs = int_to_limbs(inv_new_b, k+1)
    ok5 = (invb_limbs == REF[:k+1])
    print(f"[5] Newton formula w/ EXACT inv0 == exact floor(B^{2*k}/M)? {ok5}")
    if not ok5:
        for i in range(k+1):
            if invb_limbs[i] != REF[i]:
                print(f"    first diff (formula vs REF) at limb {i}: formula={invb_limbs[i]} got={REF[i]}")
                break
    print(f"[summary] INV[0]={INV[0]} REF[0]={REF[0]} INVvsREF match limbs1.. = {INV[1:]==REF[1:]}")
