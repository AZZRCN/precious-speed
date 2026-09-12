import re
B = 65536
data = {}
with open("muinv_blk0.txt") as f:
    for line in f:
        line = line.strip()
        if not line or "=" not in line:
            continue
        import re
        for k, v in re.findall(r"(\w+)=([0-9,]+)", line):
            if k in ("LEN2", "THIS_IN", "IN"):
                data[k] = int(v)
            else:
                data[k] = [int(x) for x in v.split(",") if x != ""]
DH = data["DH"]; INV = data["INV"]; TRUEQ = data["TRUEQ"]; DHIGH = data["DHIGH"]
len2, this_in, in_ = data["LEN2"], data["THIS_IN"], data["IN"]
print("sizes: DH", len(DH), "INV", len(INV), "TRUEQ", len(TRUEQ), "DHIGH", len(DHIGH), "len2", len2, "this_in", this_in, "in", in_)
N = sum(v*B**i for i,v in enumerate(DH))
inv = sum(v*B**i for i,v in enumerate(INV))
dhigh = sum(v*B**i for i,v in enumerate(DHIGH))
# inverse magnitude check: inv * dhigh should be ~ B^(2*in)
chk = inv * dhigh
print("inv*dhigh bits:", chk.bit_length(), "expected ~", 2*in_*16, " top limb idx ~", 2*in_)
print("B^(2*in) limb idx:", 2*in_, " chk == B^(2*in)?", chk == B**(2*in_))
print("chk // B^(2*in) (should be 0 or 1):", chk // (B**(2*in_)))
# test exponents
for E in (in_-1, in_, in_+1, in_+2):
    q = N * inv // (B**E)
    ql = []
    x = q
    while x > 0:
        ql.append(x % B); x //= B
    m = min(len(ql), len(TRUEQ))
    diff = [i for i in range(m) if ql[i] != TRUEQ[i]]
    print(f"E={E}: qhat_len={len(ql)} first_diff={diff[0] if diff else 'NONE'} numdiff={len(diff)} match={ql[:len(TRUEQ)]==TRUEQ[:len(ql)] and len(ql)==len(TRUEQ)}")
print("TRUEQ lo8:", TRUEQ[:8])
