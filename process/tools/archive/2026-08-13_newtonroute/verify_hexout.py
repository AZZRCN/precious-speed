import os
IND = 'cases_hex'
OUTD = 'cases_hex_out'
bad = []
ok = 0
for fn in sorted(os.listdir(IND)):
    if not fn.endswith('.in'):
        continue
    name = fn[:-3]
    with open(os.path.join(IND, fn)) as f:
        lines = f.read().split('\n')
    T = int(lines[0])
    pairs = [l.split() for l in lines[1:T + 1] if l.strip()]
    outp = os.path.join(OUTD, name + '.out')
    if not os.path.exists(outp):
        bad.append((name, 'NO_OUT'))
        continue
    with open(outp) as f:
        olines = f.read().split('\n')
    ores = [l.split() for l in olines if l.strip()]
    if len(ores) != len(pairs):
        bad.append((name, 'LEN %d/%d' % (len(ores), len(pairs))))
        continue
    fail = 0
    for (a, b), (q, r) in zip(pairs, ores):
        A = int(a, 16)
        B = int(b, 16)
        Q = int(q, 16)
        R = int(r, 16)
        if Q * B + R != A or not (0 <= R < B):
            fail += 1
            if fail <= 2:
                bad.append((name, 'badpair A=%s.. B=%s..' % (a[:12], b[:12])))
    if fail == 0:
        ok += 1
    elif fail > 2:
        bad.append((name, '%d bad pairs' % fail))
print('OK groups:', ok)
print('BAD:', bad)
