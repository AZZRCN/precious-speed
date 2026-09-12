import re, sys
sys.set_int_max_str_digits(400000)
t = open(r'D:\precious_speed\tools\ndiag6_out.txt').read()
cases = re.split(r'(?=^CASE )', t, flags=re.M)
for c in cases:
    m = re.search(r'CASE (\d+) n=(\d+) qn=(\d+) QEST=([0-9A-Fa-f]+)', c)
    if not m:
        continue
    idx = int(m.group(1)); n = int(m.group(2)); qn = int(m.group(3))
    qest = int(m.group(4), 16)
    tq = re.search(r'  TRUEQ=([0-9A-Fa-f]+)', c)
    trueq = int(tq.group(1), 16)
    d = qest - trueq
    s = 'QEST<TRUE' if d < 0 else ('QEST>TRUE' if d > 0 else 'EQ')
    small = abs(d).bit_length() <= 4
    print(f'CASE {idx}: n={n} qn={qn} {s} |d|bits={abs(d).bit_length()}  {"WITHIN_TOL" if small else "FAR_OFF"}')
