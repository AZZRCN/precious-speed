BASE = 65536

def to_int(limbs):
    v = 0
    for i, x in enumerate(limbs):
        v += x * (BASE ** i)
    return v

def parse(path):
    d = {}
    for line in open(path):
        line = line.strip()
        if line.startswith("K="):
            parts = line.split()
            for p in parts:
                if "=" in p:
                    kk, vv = p.split("=")
                    d[kk] = int(vv)
        elif line.startswith("M="):
            d['M'] = [int(x) for x in line[2:].split(",") if x != ""]
        elif line.startswith("INV0="):
            d['INV0'] = [int(x) for x in line[5:].split(",") if x != ""]
        elif line.startswith("PROD="):
            d['PROD'] = [int(x) for x in line[5:].split(",") if x != ""]
        elif line.startswith("INV="):
            d['INV'] = [int(x) for x in line[4:].split(",") if x != ""]
        elif line.startswith("REF="):
            d['REF'] = [int(x) for x in line[4:].split(",") if x != ""]
    return d

d = parse("D:/precious_speed/hex_best/invnewt_blk.txt")
k, s = d['K'], d['S']
inv0_len, prod_size = d['INV0LEN'], d['PRODSIZE']
M, INV0, PROD = d['M'], d['INV0'], d['PROD']
INV, REF = d['INV'], d['REF']
print(f"K={k} S={s} inv0_len={inv0_len} prod_size={prod_size}")
print(f"len M={len(M)} INV0={len(INV0)} PROD={len(PROD)} INV={len(INV)} REF={len(REF)}")

m = to_int(M); inv0 = to_int(INV0); prod = to_int(PROD)
inv = to_int(INV); ref = to_int(REF)

# 1) check inv0^2 * m == PROD
calc_prod = inv0 * inv0 * m
print("inv0^2 * m == PROD ?", calc_prod == prod)
if calc_prod != prod:
    cp = [ (calc_prod // (BASE**i)) % BASE for i in range(prod_size+2) ]
    fd = next((i for i in range(min(len(cp),len(PROD))) if cp[i]!=PROD[i]), -1)
    print("  firstdiff PROD at limb", fd, "calc=", cp[fd] if fd>=0 else None, "got=", PROD[fd] if fd>=0 else None)

# 2) check high part: expected high = floor(prod / B^(2*(k-s)))  then this is the value subtracted
shift = 2*(k-s)
high_val = prod // (BASE ** shift)
exp_inv = (2*inv0) - high_val
print("2*inv0 - high(inv0^2*m) == REF ?", exp_inv == ref)
print("2*inv0 - high(...) == INV ?", exp_inv == inv)
print("REF == inv ?", ref == inv)
# show diff
print("exp_inv - ref =", exp_inv - ref)
print("inv - ref =", inv - ref)
