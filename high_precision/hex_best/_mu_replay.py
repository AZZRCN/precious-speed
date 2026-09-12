import subprocess, random, os, re, sys
sys.set_int_max_str_digits(2000000)

EXE = r"D:\precious_speed\hex_best\_cyc_test.exe"
PY = 1

random.seed(20260813)
def gen_case(nb, na_ratio):
    B = random.getrandbits(64 * nb)
    if B < (1 << (64 * (nb - 1))): B |= (1 << (64 * (nb - 1)))
    na = max(3 * nb, int(nb * na_ratio))
    A = random.getrandbits(64 * na)
    if A < (1 << (64 * (na - 1))): A |= (1 << (64 * (na - 1)))
    if A < B: A += B
    return A, B

nb = int(sys.argv[1]) if len(sys.argv) > 1 else 200
ratio = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
A, B = gen_case(nb, ratio)
na = max(3*nb, int(nb*ratio))

# normalization (match C++: sigma = lzcnt(d[nb-1]))
top = (B >> (64*(nb-1))) & 0xFFFFFFFFFFFFFFFF
sigma = 0
v = top
while v and not (v & (1 << 63)):
    v <<= 1; sigma += 1
BS = B << sigma            # nb limbs, top bit set
qn = na - nb

inp = "1\n" + format(A, 'X') + " " + format(B, 'X') + "\n"
env = {**os.environ, "MU": "1", "MUDBG": "1"}
proc = subprocess.run([EXE], input=inp, capture_output=True, text=True, env=env)
lines = proc.stderr.split("\n")

blocks = []
i = 0
while i < len(lines):
    L = lines[i]
    if L.startswith("[BLK] "):
        m = re.search(r"wstart=(\d+) thi=(\d+) nb=(\d+) in=(\d+)", L)
        blk = {"wstart": int(m.group(1)), "thi": int(m.group(2)),
               "nb": int(m.group(3)), "in": int(m.group(4))}
        # next line should be [BLKW]<hex>
        assert lines[i+1].startswith("[BLKW]"), lines[i+1][:40]
        blk["W"] = lines[i+1][6:].strip()
        i += 2
        # scan forward for [BLKQ]
        while i < len(lines) and not lines[i].startswith("[BLKQ]"):
            i += 1
        m2 = re.search(r"corr1=(\d+) corr2=(\d+) qn_rem=(\d+)", lines[i])
        blk["corr1"] = int(m2.group(1)); blk["corr2"] = int(m2.group(2))
        blk["qn_rem"] = int(m2.group(3))
        blk["qhat"] = lines[i+1].strip()
        blocks.append(blk)
        i += 2
        continue
    i += 1

def h2i(s):
    return int(s, 16) if s else 0

print(f"nb={nb} na={na} qn={qn} in={blocks[0]['in']} sigma={sigma} #blocks={len(blocks)}")

# ---- 1) per-block qhat vs exact floor(W/BS), using C++-dumped W (真窗口) ----
allq = True
for k, blk in enumerate(blocks):
    W = h2i(blk["W"])
    qexact = W // BS
    qgot = h2i(blk["qhat"])
    ok = (qgot == qexact)
    if not ok: allq = False
    print(f"  BLK{k}: qn_rem={blk['qn_rem']} wstart={blk['wstart']} thi={blk['thi']} "
          f"corr1={blk['corr1']} corr2={blk['corr2']} qhat==floor(W/BS)? {ok}"
          + ("" if ok else f"  diff={qexact-qgot}")
          + f"  qhat_topbits={qgot >> (64*blk['thi'])}")
print("per-block qhat all exact?", allq)

# ---- 2) 组装规则对照 ----
Qexp = A // B
Rexp = A % B

def assemble(rule):
    q = [0]*(qn+2)
    for k, blk in enumerate(blocks):
        thi = blk["thi"]; wstart = blk["wstart"]; qn_rem = blk["qn_rem"]
        qh = h2i(blk["qhat"])
        limbs = [(qh >> (64*j)) & 0xFFFFFFFFFFFFFFFF for j in range(thi+1)]
        if rule == "overwrite":          # 当前 C++ 行为
            for j in range(thi+1): q[wstart+j] = limbs[j]
        elif rule == "skiptop":          # 提议修复: 仅首块写顶 limb
            for j in range(thi): q[wstart+j] = limbs[j]
            if qn_rem == qn: q[qn] = limbs[thi]
        elif rule == "add":              # 加法组装 (带进位)
            car = 0
            for j in range(thi+1):
                s = q[wstart+j] + limbs[j] + car
                q[wstart+j] = s & 0xFFFFFFFFFFFFFFFF
                car = s >> 64
            j = thi+1
            while car:
                s = q[wstart+j] + car
                q[wstart+j] = s & 0xFFFFFFFFFFFFFFFF; car = s >> 64; j += 1
    val = 0
    for j in range(len(q)-1, -1, -1):
        val = (val << 64) | q[j]
    return val

for rule in ("overwrite", "skiptop", "add"):
    Qa = assemble(rule)
    d = Qexp - Qa
    print(f"  rule={rule:10s} Q==exact? {Qa == Qexp}"
          + ("" if d == 0 else f"  delta_bits={d.bit_length()} sign={'+' if d>0 else '-'}"))
