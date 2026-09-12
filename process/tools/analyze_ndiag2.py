import re, sys
sys.set_int_max_str_digits(300000)
t = open(r'D:\precious_speed\tools\ndiag2_out.txt').read()
cases = re.split(r'(?=^CASE )', t, flags=re.M)
for c in cases:
    m = re.search(r'CASE (\d+) NA=(\d+) NB=(\d+) qn=(\d+) MATCH=(\d) CONSIST=(\d)', c)
    if not m:
        continue
    idx, na, nb, qn, match, consist = m.groups()
    if match == '1':
        print(f'CASE {idx}: NA={na} NB={nb} qn={qn} MATCH=1 CONSIST=1 [OK]')
        continue
    print(f'CASE {idx}: NA={na} NB={nb} qn={qn} MATCH={match} CONSIST={consist} [FAIL]')
    qk = re.search(r'  Qk=([0-9A-Fa-f]+)', c)
    qn_ = re.search(r'  Qn=([0-9A-Fa-f]+)', c)
    if qk and qn_:
        a = int(qk.group(1), 16)
        b = int(qn_.group(1), 16)
        d = b - a  # newton - truth
        sign = 'NEW>TRUTH' if d > 0 else ('NEW<TRUTH' if d < 0 else 'EQUAL')
        print(f'   Q: {sign}  |diff| bits={d.bit_length()}  truth_bits={a.bit_length()}')
        # how many low limbs match?
        x = abs(d)
        lowzero = 0
        while x & 1 == 0 and x:
            lowzero += 1; x >>= 1
        print(f'   |diff| low_zero_bits={lowzero}  (~{lowzero//4} hex digits / {lowzero//64} limbs from bottom)')
