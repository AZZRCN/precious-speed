#!/usr/bin/env python3
import subprocess, sys

# Boundary tests around 16-limb AVX2 boundary (16 limbs = 64 decimal digits)
# Also tests scalar 8-way boundary (8 limbs = 32 digits) and 1-limb (4 digits)

add_tests = [
    ('0', '0', 'zero'),
    ('1', '1', '1digit'),
    ('9999', '1', '4digit_carry'),
    ('9999', '9999', '4digit_full'),
    ('10000', '1', '5digit'),
    ('99999999', '1', '8digit_carry'),
    ('99999999', '99999999', '8digit_full'),
    ('100000000', '1', '9digit'),
    ('9' * 16, '1', '16digit_carry'),
    ('9' * 16, '9' * 16, '16digit_full'),
    ('9' * 17, '1', '17digit_carry'),
    ('9' * 25, '1', '25digit_carry'),
    ('9' * 32, '1', '32digit_carry'),
    ('9' * 32, '9' * 32, '32digit_full'),
    ('9' * 64, '1', '64digit_carry'),
    ('9' * 64, '9' * 64, '64digit_full'),
    ('9' * 65, '1', '65digit_carry'),
    ('9' * 80, '1', '80digit_carry'),
    ('9' * 128, '1', '128digit_carry'),
    ('9' * 100, '9' * 100, '100digit_full'),
    ('-' + '9' * 64, '1', 'neg64_add'),
    ('-' + '9' * 64, '-' + '9' * 64, 'neg64_neg64'),
    ('1', '-' + '9' * 64, 'pos1_neg64'),
    ('9' * 64, '-' + '1', 'pos64_neg1'),
    ('9' * 64, '-' + '9' * 64, 'pos64_neg64'),
    ('1' + '0' * 63, '-' + '1', 'pow64_neg1'),
]

fails = 0
for a, b, desc in add_tests:
    inp = '1\n' + a + '\n' + b + '\n'
    r = subprocess.run(['./mf_ADD'], input=inp.encode(), capture_output=True)
    out = r.stdout.decode().strip()
    exp = str(int(a) + int(b))
    if out != exp:
        fails += 1
        print('FAIL[%s]: %s + %s = %s (exp %s)' % (desc, a[:30], b[:30], out[:40], exp[:40]))
print('ADD edge: %d/%d failed' % (fails, len(add_tests)))

div_tests = [
    ('9' * 64, '1', 'div64_1'),
    ('9' * 64, '9' * 32, 'div64_32'),
    ('9' * 65, '9' * 32, 'div65_32'),
    ('9' * 80, '9' * 64, 'div80_64'),
    ('1' + '0' * 64, '9' * 32, 'div_pow65_32'),
    ('9' * 100, '9' * 50, 'div100_50'),
    ('9' * 128, '9' * 64, 'div128_64'),
    ('-' + '9' * 64, '9' * 32, 'div_neg64_32'),
    ('9' * 64, '-' + '9' * 32, 'div64_neg32'),
]
dfails = 0
for a, b, desc in div_tests:
    inp = '1\n' + a + '\n' + b + '\n'
    r = subprocess.run(['./mf_DIV'], input=inp.encode(), capture_output=True)
    out = r.stdout.decode().strip()
    ai, bi = int(a), int(b)
    q = ai // bi
    rem = ai % bi
    exp = str(q) + ' ' + str(rem)
    if out != exp:
        dfails += 1
        print('FAIL[%s]: %s / %s = %s (exp %s)' % (desc, a[:30], b[:30], out[:40], exp[:40]))
print('DIV edge: %d/%d failed' % (dfails, len(div_tests)))

mul_tests = [
    ('9' * 64, '9' * 64, 'mul64_64'),
    ('9' * 65, '9' * 65, 'mul65_65'),
    ('9' * 32, '9' * 32, 'mul32_32'),
    ('-' + '9' * 64, '9' * 64, 'mul_neg64_64'),
]
mfails = 0
for a, b, desc in mul_tests:
    inp = '1\n' + a + '\n' + b + '\n'
    r = subprocess.run(['./mf_MUL'], input=inp.encode(), capture_output=True)
    out = r.stdout.decode().strip()
    exp = str(int(a) * int(b))
    if out != exp:
        mfails += 1
        print('FAIL[%s]: %s * %s = %s (exp %s)' % (desc, a[:30], b[:30], out[:40], exp[:40]))
print('MUL edge: %d/%d failed' % (mfails, len(mul_tests)))

total = fails + dfails + mfails
print('\nTOTAL: %d failures' % total)
sys.exit(1 if total else 0)
