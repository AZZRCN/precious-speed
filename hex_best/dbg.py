import random, subprocess, sys

def rh(nb):
    v = random.getrandbits(nb)
    if v == 0:
        v = 1
    return format(v, 'X')

def gc(sizes, n_per):
    cs = []
    for nb in sizes:
        for _ in range(n_per):
            A = rh(nb)
            bb = max(1, random.randint(1, nb))
            B = rh(bb)
            if B in ('0', ''):
                B = '1'
            cs.append((A, B))
    return cs

random.seed(0)
sizes = [64, 256, 1024, 4096, 8192, 16384, 32768, 65536]
n_per = 3
cs = gc(sizes, n_per)
for idx, (A, B) in enumerate(cs):
    with open('/tmp/d.txt', 'w') as f:
        f.write('1\n' + A + ' ' + B + '\n')
    oa = subprocess.run(['./div_base16'], stdin=open('/tmp/d.txt'), capture_output=True, text=True).stdout.strip()
    ob = subprocess.run(['./div_best'], stdin=open('/tmp/d.txt'), capture_output=True, text=True).stdout.strip()
    a = int(A, 16)
    b = int(B, 16)
    msg = ''
    pa = oa.split()
    if len(pa) == 2:
        q = int(pa[0], 16)
        r = int(pa[1], 16)
        if r >= b:
            msg = 'base16 R>=B (R=0x%x b=0x%x)' % (r, b)
        elif q * b + r != a:
            msg = 'base16 Q*B+R!=A'
    else:
        msg = 'base16 bad line %r' % oa
    if oa != ob or msg:
        print('[%d] lenA=%d lenB=%d A>=B=%s' % (idx, len(A), len(B), a >= b))
        print('  A=%s' % A)
        print('  B=%s' % B)
        print('  base16=%s' % oa)
        print('  best  =%s' % ob)
        print('  msg=%s' % msg)
        sys.stdout.flush()
print('done seed0')
