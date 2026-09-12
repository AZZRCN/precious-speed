import re
B = 65536
data = {}
with open("muinv_blk0.txt") as f:
    for line in f:
        line = line.strip()
        if not line or "=" not in line:
            continue
        for k, v in re.findall(r"(\w+)=([0-9,]+)", line):
            if k in ("LEN2", "THIS_IN", "IN"):
                data[k] = int(v)
            else:
                data[k] = [int(x) for x in v.split(",") if x != ""]
D = data["DIVISOR"]; INV = data["INV"]; DHIGH = data["DHIGH"]
len2, in_ = data["LEN2"], data["IN"]
inv_extra = 2
def to_int(a): return sum(v*B**i for i,v in enumerate(a))
D_high = to_int(DHIGH)
inv_needed = B**(2*in_) // D_high   # what mu formula needs
# code computes inverse of divisor high (in+inv_extra) limbs, truncated by inv_extra
D_ine2 = to_int(D[len2 - (in_+inv_extra):])  # top in+2 limbs
inv_raw = B**(2*(in_+inv_extra)) // D_ine2
inv_trunc = inv_raw // (B**inv_extra)  # drop low inv_extra limbs
def limbs(x):
    l=[]; 
    while x>0: l.append(x%B); x//=B
    return l
inv_needed_l = limbs(inv_needed)
inv_trunc_l = limbs(inv_trunc)
inv_dump_l = INV
print("inv_needed len:", len(inv_needed_l), "inv_trunc len:", len(inv_trunc_l), "dump len:", len(inv_dump_l))
# compare dump with inv_trunc (what code should produce)
m = min(len(inv_trunc_l), len(inv_dump_l))
diff_t = [i for i in range(m) if inv_trunc_l[i]!=inv_dump_l[i]]
print("dump vs inv_trunc: first_diff=", diff_t[0] if diff_t else "NONE", "numdiff=", len(diff_t))
# compare dump with inv_needed
m2 = min(len(inv_needed_l), len(inv_dump_l))
diff_n = [i for i in range(m2) if inv_needed_l[i]!=inv_dump_l[i]]
print("dump vs inv_needed: first_diff=", diff_n[0] if diff_n else "NONE", "numdiff=", len(diff_n))
# magnitude of dump vs needed
dump_int = to_int(inv_dump_l)
print("ratio dump/inv_needed:", dump_int / inv_needed)
print("ratio inv_trunc/inv_needed:", inv_trunc / inv_needed)
# print low 6 limbs of each
print("dump lo6:", inv_dump_l[:6])
print("inv_trunc lo6:", inv_trunc_l[:6])
print("inv_needed lo6:", inv_needed_l[:6])
