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

sizes = [64, 256, 1024, 4096, 8192, 16384, 32768, 65536]
n_per = 4
total_fail = 0
for seed in range(8):
    random.seed(seed)
    cs = gc(sizes, n_per)
    for idx, (A, B) in enumerate(cs):
        with open('/tmp/d.txt', 'w') as f:
            f.write('1\n' + A + ' ' + B + '\n')
        oa = subprocess.run(['./div_base16'], stdin=open('/tmp/d.txt'), capture_output=True, text=True).stdout.strip()
        ob = subprocess.run(['./div_best'], stdin=open('/tmp/d.txt'), capture_output=True, text=True).stdout.strip()
        a = int(A, 16)
        b = int(B, 16)
        ok = True
        pa = oa.split()
        if len(pa) == 2:
            q = int(pa[0], 16)
            r = int(pa[1], 16)
            if r >= b or q * b + r != a:
                ok = False
        else:
            ok = False
        if oa != ob or not ok:
            total_fail += 1
            if total_fail <= 20:
                print('seed%d[%d] lenA=%d lenB=%d oa_eq_ob=%s ok=%s' % (seed, idx, len(A), len(B), oa == ob, ok))
                sys.stdout.flush()
print('TOTAL_FAIL=%d' % total_fail)
