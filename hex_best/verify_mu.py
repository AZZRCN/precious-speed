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
W = data["WINDOW"]; D = data["DIVISOR"]; DH = data["DH"]; INV = data["INV"]
TRUEQ = data["TRUEQ"]; DHIGH = data["DHIGH"]
len2, this_in, in_ = data["LEN2"], data["THIS_IN"], data["IN"]
print("sizes: W", len(W), "D", len(D), "DH", len(DH), "INV", len(INV), "TRUEQ", len(TRUEQ), "DHIGH", len(DHIGH))
print("len2", len2, "this_in", this_in, "in", in_)
# expected window size = len2 + this_in
print("window size check:", len(W) == len2 + this_in)
# N_high expected = top this_in+1 limbs of W
exp_Nhigh = W[len(W) - (this_in + 1):] if this_in + 1 <= len(W) else None
print("DH == top(this_in+1) of W?", exp_Nhigh is not None and exp_Nhigh == DH)
# direct block quotient in python
def to_int(a):
    return sum(v*B**i for i,v in enumerate(a))
Wv = to_int(W); Dv = to_int(D)
true_block = Wv // Dv
tl = []
x = true_block
while x > 0:
    tl.append(x % B); x //= B
print("python floor(W/D) len:", len(tl), " vs TRUEQ len:", len(TRUEQ))
print("python floor(W/D) lo8:", tl[:8])
print("TRUEQ lo8:", TRUEQ[:8])
print("floor(W/D) == TRUEQ?", tl[:len(TRUEQ)] == TRUEQ[:len(tl)] and len(tl) == len(TRUEQ))
# inverse check
invv = to_int(INV); dhighv = to_int(DHIGH)
chk = invv * dhighv
print("inv*dhigh bits:", chk.bit_length(), "vs 2*in*16=", 2*in_*16, " ratio chk/B^(2in)=", chk / (B**(2*in_)))
# qhat formula test
Nv = to_int(DH)
for E in (in_-2, in_-1, in_, in_+1, in_+2):
    q = Nv * invv // (B**E)
    ql = []
    x = q
    while x > 0:
        ql.append(x % B); x //= B
    m = min(len(ql), len(TRUEQ))
    diff = [i for i in range(m) if ql[i] != TRUEQ[i]]
    print(f"  E={E}: len={len(ql)} first_diff={diff[0] if diff else 'NONE'} match={ql[:len(TRUEQ)]==TRUEQ[:len(ql)] and len(ql)==len(TRUEQ)}")
