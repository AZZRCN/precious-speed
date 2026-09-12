import re, sys
sys.set_int_max_str_digits(800000)
BETA = 1 << 64
t = open(r'D:\precious_speed\tools\ndiag3_out.txt').read()
blocks = re.split(r'(?=^CASE )', t, flags=re.M)
for c in blocks:
    m = re.search(r'CASE (\d+) n=(\d+) sigma=(\d+) V=([0-9A-Fa-f]+)', c)
    if not m: continue
    idx, n, sigma, vhex = m.groups()
    idx = int(idx); n = int(n)
    dn_m = re.search(r'  DN=([0-9A-Fa-f]+)', c)
    if not dn_m:
        print(idx, "no DN"); continue
    dn_int = int(dn_m.group(1), 16)
    bn = BETA ** n
    b2n = BETA ** (2 * n)
    true_v = (b2n // dn_int) - bn
    vgot = int(vhex, 16)
    # also: does dn have top bit set?
    top_set = (dn_int >> (64*(n-1) + 63)) & 1
    diff = true_v - vgot
    status = 'OK' if (diff >= 0 and abs(diff).bit_length() <= 4) else ('UNDER' if diff > 0 else 'OVER')
    print(f"CASE {idx}: n={n} sigma={sigma} topbit_set={top_set} {status} |diff|bits={abs(diff).bit_length()}")
    if status != 'OK':
        print(f"    vgot_top_limb={vgot >> (64*(n-1)):016x}  true_top_limb={true_v >> (64*(n-1)):016x}")
        print(f"    vgot_bits={vgot.bit_length()}  true_bits={true_v.bit_length()}")
