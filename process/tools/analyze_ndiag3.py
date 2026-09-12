import re, sys
sys.set_int_max_str_digits(400000)
t = open(r'D:\precious_speed\tools\ndiag3_out.txt').read()
# also need the actual divisors B from divlarge_in.txt to compute ground truth dn
lines = open(r'D:\precious_speed\tools\divlarge_in.txt').read().split('\n')
# line0 = T, then pairs
pairs = []
for ln in lines[1:]:
    ln = ln.strip()
    if not ln: continue
    a, b = ln.split()
    pairs.append((a, b))
    if len(pairs) >= 8: break

blocks = re.split(r'(?=^CASE )', t, flags=re.M)
for c in blocks:
    m = re.search(r'CASE (\d+) n=(\d+) sigma=(\d+) V=([0-9A-Fa-f]+)', c)
    if not m: continue
    idx, n, sigma, vhex = m.groups()
    idx = int(idx); n = int(n)
    dn_m = re.search(r'  DN=([0-9A-Fa-f]+)', c)
    # recompute dn from B (the divisor) to get ground truth
    _, Bhex = pairs[idx]
    Bint = int(Bhex, 16)
    # normalize: shift left so top bit set
    # compute sigma = 0 if already normalized? We'll replicate: dn = B << sigma where sigma = lzcnt(top limb)
    # but easier: dn_int = Bint << sigma, with sigma = (64 - Bint.bit_length()%64)%64
    blen = Bint.bit_length()
    sigma2 = (64 - (blen % 64)) % 64
    dn_int = Bint << sigma2
    # ground truth v = floor(beta^(2n)/dn) - beta^n, beta=2^64
    beta = 1 << 64
    bn = beta ** n
    b2n = beta ** (2 * n)
    true_v = (b2n // dn_int) - bn
    vgot = int(vhex, 16)
    diff = true_v - vgot   # >0 means got is underestimate
    sign = 'UNDER' if diff > 0 else ('OVER' if diff < 0 else 'EXACT')
    print(f'CASE {idx}: n={n} sigma={sigma}(sigma2={sigma2}) Vbits={vgot.bit_length()} trueVbits={true_v.bit_length()}  {sign} |diff|bits={abs(diff).bit_length()}')
    if abs(diff).bit_length() > 4:
        # show low limbs diff to see if it's just rounding
        print(f'    low hex diff: got={vhex[-48:]} true={format(true_v, "x")[-48:]}')
