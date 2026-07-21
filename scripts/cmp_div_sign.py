#!/usr/bin/env python3
import subprocess
# Compare DIV sign handling: previous binary vs new binary
cases = [
    ('-' + '9' * 64, '9' * 32, 'neg64_pos32'),
    ('9' * 64, '-' + '9' * 32, 'pos64_neg32'),
    ('-' + '9' * 64, '-' + '9' * 32, 'neg64_neg32'),
    ('9' * 64, '9' * 32, 'pos64_pos32'),
]
for a, b, desc in cases:
    inp = ('1\n%s\n%s\n' % (a, b)).encode()
    prev = subprocess.run(['./mf_DIV.prev'], input=inp, capture_output=True).stdout.decode().strip()
    new = subprocess.run(['./mf_DIV'], input=inp, capture_output=True).stdout.decode().strip()
    exp_q = int(a) // int(b)
    exp_r = int(a) % int(b)
    exp = '%d %d' % (exp_q, exp_r)
    match = 'SAME' if prev == new else 'DIFF'
    okp = 'OK' if prev == exp else 'FAIL'
    okn = 'OK' if new == exp else 'FAIL'
    print('[%s] exp=%s' % (desc, exp))
    print('  prev=%s [%s]' % (prev, okp))
    print('  new =%s [%s]  (%s)' % (new, okn, match))
