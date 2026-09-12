import sys
sys.set_int_max_str_digits(800000)

def h2i(s):
    val = 0
    for i in range(0, len(s), 16):
        val = (val << 64) | int(s[i:i+16], 16)
    return val

tags = ('DN:', 'AN:', 'V:', 'V2:', 'L:', 'QE:', 'RT:', 'Qn:', 'Qk:', 'Rk:')
def tagof(l):
    for t in tags:
        if l.startswith(t): return t[:-1]
    return None

lines = open(r'D:\precious_speed\tools\ndiag9_out.txt', encoding='utf-8', errors='replace').read().split('\n')

blocks = []
cur = None
for l in lines:
    if l.startswith('CASE '):
        if cur: blocks.append(cur)
        cur = {'hdr': l, 'd': {}}
    else:
        t = tagof(l)
        if t and cur is not None and t not in cur['d']:
            cur['d'][t] = l[len(t)+1:]   # keep first occurrence only
if cur: blocks.append(cur)

B = 1 << 64
for c in blocks:
    hdr = c['hdr']
    print('=' * 80)
    print(hdr)
    d = c['d']
    if 'DN' not in d:
        print('  (no dump — skipped)')
        continue
    dn = h2i(d['DN']); an = h2i(d['AN'])
    v = h2i(d['V']); v2 = h2i(d['V2'])
    L = h2i(d['L']); qe = h2i(d['QE']); rt = h2i(d['RT'])
    Qn = h2i(d['Qn']); Qk = h2i(d['Qk']); Rk = h2i(d['Rk'])
    n = len(d['DN']) // 16
    na_an = len(d['AN']) // 16
    Llimbs = len(d['L']) // 16
    qn = len(d['Qn']) // 16
    nb = len(d['Rk']) // 16
    print(f"  n={n} na_an={na_an} Llimbs={Llimbs} qn={qn} nb={nb}")
    v_true = (B**(2*n) // dn) - (B**n)
    print(f"  V == v_true ? {v == v_true}")
    print(f"  V2 == v_true? {v2 == v_true}")
    if v != v_true:
        print(f"  |V - v_true| bits = {(v ^ v_true).bit_length()}")
    modL = B**(64*Llimbs)
    print(f"  L == an*v ? {L == (an*v) % modL}")
    # recompute q_est
    himid = an >> (64*n)
    prod = an * v
    lo = prod >> (128*n)
    q0 = himid + lo
    amod = an & ((B**(2*n)) - 1)
    Lmod = (an*v) & ((B**(2*n)) - 1)
    if amod + Lmod >= (B**(2*n)):
        q0 += 1
    print(f"  qe == recomputed q_est ? {qe == q0}")
    if qe != q0:
        print(f"  qe bits={qe.bit_length()}  q0 bits={q0.bit_length()}  diff bits={(qe^q0).bit_length()}")
    print(f"  Qn == Qk (true quotient) ? {Qn == Qk}")
    if Qn != Qk:
        print(f"  Qn bits={Qn.bit_length()} Qk bits={Qk.bit_length()} |diff| bits={(Qn^Qk).bit_length()}")
    # consistency in normalized space
    print(f"  qe*dn + rt == an ? {(qe*dn + rt) == an}")
