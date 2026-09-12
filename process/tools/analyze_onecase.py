import re, sys
sys.set_int_max_str_digits(800000)

# true q from stdout
out = open(r'D:\precious_speed\tools\onecase_nd_out.txt').read()
qk_m = re.search(r'  Qk=([0-9A-Fa-f]+)', out)
qk = int(qk_m.group(1), 16)
qn_m = re.search(r'qn=(\d+)', out)
qn = int(qn_m.group(1))

err = open(r'D:\precious_speed\tools\onecase_nd_err.txt').read()

def get(tag):
    m = re.search(tag + r'=([0-9A-Fa-f]+)', err)
    return int(m.group(1), 16) if m else None

V = get('V'); L = get('L'); QEST = get('QEST'); QE_ifb = get('QE_ifb')
RT_ifb = get('RT_ifb'); QE_FINAL = get('QE_FINAL'); RT_FINAL = get('RT_FINAL')

print(f"true q (Qk) bits = {qk.bit_length()}")
print(f"qn = {qn}")
for name, val in [('V',V),('L',L),('QEST',QEST),('QE_ifb',QE_ifb),('QE_FINAL',QE_FINAL)]:
    if val is None: print(f"  {name}: MISSING"); continue
    print(f"  {name}: bits={val.bit_length()}")
    if name in ('QEST','QE_ifb','QE_FINAL'):
        d = val - qk
        print(f"      diff_from_true = {d}  (bits={abs(d).bit_length()})")

# sanity: check V is a valid reciprocal underestimate: d * (beta^n + V) <= beta^{2n}?
# we don't have d here; but check V's magnitude: V should be ~ beta^n (n limbs)
print("\n--- L = an*v, check: floor(L/beta^{2n}) + H should equal QEST ---")
# We can at least check QE_FINAL vs Qk
if QE_FINAL is not None:
    df = QE_FINAL - qk
    print(f"QE_FINAL vs true: diff bits={abs(df).bit_length()}  sign={'over' if df>0 else 'under'}")
