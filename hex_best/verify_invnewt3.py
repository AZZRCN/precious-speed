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

blocks = parse_block(sys.argv[1])
print(f"parsed {len(blocks)} block(s)")
for blk in blocks:
    K = blk["K"]; S = blk["S"]; INV0LEN = blk["INV0LEN"]; PRODSIZE = blk["PRODSIZE"]
    M = blk["M"]; REALINV0 = blk["REALINV0"]; SQRT = blk["SQRT"]
    PROD = blk["PROD"]; INV = blk["INV"]; REF = blk["REF"]
    Mval = limbs_to_int(M)
    Msub = M[S:]
    Msub_val = limbs_to_int(Msub)
    k = K
    ks = k - S
    print(f"\n=== K={K} S={S} INV0LEN={INV0LEN} PRODSIZE={PRODSIZE} ===")
    inv0_exact = Msub_val // (B ** (2*ks))
    inv0_exact_limbs = int_to_limbs(inv0_exact, INV0LEN)
    ok_inv0 = (inv0_exact_limbs == REALINV0[:INV0LEN])
    print(f"[1] REALINV0 == exact floor(B^(2*(k-s))/Msub)? {ok_inv0}")
    if not ok_inv0:
        for i in range(INV0LEN):
            if inv0_exact_limbs[i] != REALINV0[i]:
                print(f"    first diff inv0 at limb {i}: exact={inv0_exact_limbs[i]} got={REALINV0[i]}")
                break
    inv0_sq_exact = inv0_exact * inv0_exact
    sqr_limbs = int_to_limbs(inv0_sq_exact, PRODSIZE)
    ok_sqr = (sqr_limbs == SQRT[:PRODSIZE])
    print(f"[2] SQRT(inv0^2) == exact inv0^2? {ok_sqr}")
    if not ok_sqr:
        for i in range(PRODSIZE):
            if sqr_limbs[i] != SQRT[i]:
                print(f"    first diff sqr at limb {i}: exact={sqr_limbs[i]} got={SQRT[i]}")
                break
    prod_exact = inv0_sq_exact * Mval
    prod_limbs = int_to_limbs(prod_exact, PRODSIZE)
    ok_prod = (prod_limbs == PROD[:PRODSIZE])
    print(f"[3] PROD(inv0^2*M) == exact? {ok_prod}")
    if not ok_prod:
        for i in range(PRODSIZE):
            if prod_limbs[i] != PROD[i]:
                print(f"    first diff prod at limb {i}: exact={prod_limbs[i]} got={PROD[i]}")
                break
    high = prod_exact // (B ** (2*ks))
    inv_code = (B ** S) * (2 * inv0_exact) - high
    inv_code_limbs = int_to_limbs(inv_code, k+1)
    ok_inv = (inv_code_limbs == INV[:k+1])
    print(f"[4] INV(final) == formula(B^s*2inv0 - high)? {ok_inv}")
    if not ok_inv:
        for i in range(k+1):
            if inv_code_limbs[i] != INV[i]:
                print(f"    first diff INV at limb {i}: formula={inv_code_limbs[i]} got={INV[i]}")
                break
    ref_val = Mval // (B ** (2*k))
    ref_limbs = int_to_limbs(ref_val, k+1)
    ok_ref = (ref_limbs == REF[:k+1])
    print(f"[5] REF == exact floor(B^(2k)/M)? {ok_ref}")
    if not ok_ref:
        for i in range(k+1):
            if ref_limbs[i] != REF[i]:
                print(f"    first diff REF at limb {i}: exact={ref_limbs[i]} got={REF[i]}")
                break
    ok_invref = (inv_code_limbs == REF[:k+1])
    print(f"[6] formula result == REF (Newton step math correct)? {ok_invref}")
