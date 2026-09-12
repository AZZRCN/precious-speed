import random, subprocess, sys

BIN = sys.argv[1] if len(sys.argv) > 1 else './div_base16_allexact'

def rh(nb):
    v = random.getrandbits(nb)
    if v == 0:
        v = 1
    return format(v, 'X')

sizes = [64, 256, 1024, 4096, 8192, 16384, 32768, 65536]
n_per = 4
TIMEOUT = 90
for seed in range(1):  # seed 0 only for focused debug
    random.seed(seed)
    for sz_i, nb in enumerate(sizes):
        for k in range(n_per):
            idx = sz_i * 4 + k
            A = rh(nb)
            bb = max(1, random.randint(1, nb))
            B = rh(bb)
            if B in ('0', ''):
                B = '1'
            a = int(A, 16); b = int(B, 16)
            tin = '1\n' + A + ' ' + B + '\n'
            try:
                _pr = subprocess.run([BIN], input=tin, capture_output=True,
                                     text=True, timeout=TIMEOUT)
                oa = _pr.stdout.strip()
                _stde = _pr.stderr
            except subprocess.TimeoutExpired:
                print('HANG idx=%d nb=%d lenA=%d lenB=%d (A=%s B=%s)' %
                      (idx, nb, len(A), len(B), A[:40], B[:40]))
                sys.stdout.flush()
                continue
            pa = oa.split()
            okq = False
            q = r = None
            if len(pa) == 2:
                q = int(pa[0], 16); r = int(pa[1], 16)
                okq = (r < b) and (q * b + r == a)
            if not okq:
                tq, tr = divmod(a, b)
                hq = format(q, 'X'); ht = format(tq, 'X')
                m = max(len(hq), len(ht))
                hhq = hq.rjust(m, '0'); hht = ht.rjust(m, '0')
                first_diff = None
                for i in range(m):
                    if hhq[i] != hht[i]:
                        first_diff = i; break
                print('WRONG idx=%d nb=%d lenA=%d lenB=%d' % (idx, nb, len(A), len(B)))
                print('  Q_eq=%s R_eq=%s Q_hexlen=%d trueQ_hexlen=%d first_diff_pos=%s' %
                      (q == tq, r == tr, len(hq), len(ht), first_diff))
                print('  div16 Q hi=%s' % hq[:48])
                print('  true  Q hi=%s' % ht[:48])
                print('  div16 R=%s trueR=%s R_in_range=%s' %
                      (format(r, 'X'), format(tr, 'X'), (r < b) if r is not None else None))
                print('  A=%s' % A[:48])
                print('  B=%s' % B[:48])
                if _stde:
                    for _l in _stde.strip().split('\n'):
                        print('  [muinv] ' + _l)
                print('---')
                sys.stdout.flush()
print('REPRO_DONE')
