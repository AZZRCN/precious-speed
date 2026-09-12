import sys
sys.set_int_max_str_digits(800000)

# read original inputs A,B per case from divlarge_in.txt
def read_inputs(path):
    lines = [l.strip() for l in open(path, encoding='utf-8') if l.strip()]
    T = int(lines[0])
    cases = []
    for k in range(1, T+1):
        a, b = lines[k].split()
        cases.append((a, b))
    return cases

inp = read_inputs(r'D:\precious_speed\tools\divlarge_in.txt')
# fallback: also in dbg? use hex_best path
cases = inp

B = 1 << 64
def h2i(s):
    v = 0
    # s is hex string of concatenated limbs, high limb first
    # split into 16-hex chunks from the right
    s = s.strip()
    # pad to multiple of 16
    while len(s) % 16: s = '0' + s
    for i in range(0, len(s), 16):
        v = (v << 64) | int(s[i:i+16], 16)
    return v

data = open(r'D:\precious_speed\tools\ndiag14_out.txt', encoding='utf-8').read().split('\n')
i = 0
ci = 0
while i < len(data):
    line = data[i]
    if not line.startswith('C '):
        i += 1
        continue
    # header
    parts = line.split()
    t = int(parts[1])
    if parts[2] in ('S0','SA','S1','NBZ'):
        print(line, 'skipped')
        i += 1
        continue
    kv = dict(p.split('=') for p in parts if '=' in p)
    na = int(kv['na'])
    nb = int(kv['nb'])
    n = int(kv['n'])
    na_an = int(kv['na_an'])
    sig = int(kv['sigma'])
    an_hex = data[i+1][3:]   # strip 'AN '
    dn_hex = data[i+2][3:]   # strip 'DN '
    de_hex = data[i+3][3:]   # strip 'DE '
    di_hex = data[i+4][3:]   # strip 'DI '
    bo_hex = data[i+5][3:]   # strip 'BO '
    i += 6
    an = h2i(an_hex)
    dn = h2i(dn_hex)
    # original A, B
    A_hex = cases[t][0]
    B_hex = cases[t][1]
    Aint = int(A_hex, 16)
    Bint = int(B_hex, 16)
    # reconstruct sigma from B top limb
    sigma = 64 - Bint.bit_length() % 64 if Bint else 64
    # actually top limb:
    # compute via int
    btop = (Bint >> (64*(nb-1))) & ((1<<64)-1)
    sigma2 = 64 - btop.bit_length() if btop else 64
    print('='*70)
    print(f'CASE {t}: na={na} nb={nb} n={n} na_an={na_an} sigma_prog={sig} sigma_fromB={sigma2}')
    # verify normalization
    an_ok = (Aint << sig) == an
    dn_ok = (Bint << sig) == dn
    print(f'  dn == B<<sigma ? {dn_ok}   an == A<<sigma ? {an_ok}')
    if not dn_ok:
        print(f'    |dn - B<<sig| bits = {(dn ^ (Bint<<sig)).bit_length()}  dn bits={dn.bit_length()} B<<sig bits={(Bint<<sig).bit_length()}')
    if not an_ok:
        print(f'    |an - A<<sig| bits = {(an ^ (Aint<<sig)).bit_length()}')
    di = h2i(di_hex); bo = h2i(bo_hex); de = h2i(de_hex)
    print(f'  DI==BO(pristine B)? {di == bo}   DI==inputB? {di == Bint}   BO==inputB? {bo == Bint}')
    print(f'  DE(early dn)==DI<<sig ? {de == (di << sig)}')
    print(f'  DN(late dn)==DE(early) ? {dn == de}   DN==DI<<sig ? {dn == (di << sig)}')
    q_ann_dn = an // dn
    q_AB = Aint // Bint
    print(f'  an//dn == A//B ? {q_ann_dn == q_AB}')
    if q_ann_dn != q_AB:
        print(f'    an//dn bits={q_ann_dn.bit_length()}  A//B bits={q_AB.bit_length()}  diff bits={(q_ann_dn ^ q_AB).bit_length()}')
    # also check remainder consistency
    r1 = an - q_ann_dn*dn
    r2 = Aint - q_AB*Bint
    print(f'  (an//dn)*dn + r == an with r<dn? {r1 < dn}   r2<B? {r2 < Bint}')
