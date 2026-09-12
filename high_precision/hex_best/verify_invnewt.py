import re

BASE = 65536

def parse(path):
    data = {}
    cur = None
    for line in open(path):
        line = line.strip()
        if line.startswith("K="):
            data['K'] = int(line[2:])
        elif line.startswith("M="):
            cur = 'M'
            data[cur] = [int(x) for x in line[2:].split(",") if x != ""]
        elif line.startswith("INV="):
            data['INV'] = [int(x) for x in line[4:].split(",") if x != ""]
        elif line.startswith("REF="):
            data['REF'] = [int(x) for x in line[4:].split(",") if x != ""]
    return data

d = parse("D:/precious_speed/hex_best/invnewt_blk.txt")
k = d['K']
M = d['M']
INV = d['INV']
REF = d['REF']
print("K =", k, " len(M)=", len(M), " len(INV)=", len(INV), " len(REF)=", len(REF))

def to_int(limbs):
    v = 0
    for i, x in enumerate(limbs):
        v += x * (BASE ** i)
    return v

m = to_int(M)
inv = to_int(INV)
ref = to_int(REF)
B2k = BASE ** (2 * k)
true_inv = B2k // m
print("true_inv (python) == ref?", true_inv == ref)
print("true_inv (python) == inv?", true_inv == inv)
ratio = inv / true_inv if true_inv else float('nan')
print("inv / true_inv =", ratio)
# limb-by-limb firstdiff
fd = -1
for i in range(min(len(INV), len(REF))):
    if INV[i] != REF[i]:
        fd = i; break
print("firstdiff(INV,REF) =", fd)
# How does INV compare to floor(true_inv * something)? check per-limb ratio of low limbs
print("INV low 6:", INV[:6])
print("REF low 6:", REF[:6])
print("INV high 3:", INV[-3:])
print("REF high 3:", REF[-3:])
