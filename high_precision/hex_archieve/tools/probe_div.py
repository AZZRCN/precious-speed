import os, glob
d = '/home/azzr/hexbench/data/div'
for f in sorted(glob.glob(d + '/*.in')):
    n = os.path.basename(f)[:-3]
    with open(f) as fh:
        T = int(fh.readline())
        mla = mlb = 0; sla = slb = 0; k = 0; mx_qn = 0; mn_lb = 10**9
        for line in fh:
            p = line.split()
            if len(p) < 2: continue
            la, lb = len(p[0]), len(p[1])
            mla = max(mla, la); mlb = max(mlb, lb); mn_lb = min(mn_lb, lb)
            sla += la; slb += lb; k += 1
            mx_qn = max(mx_qn, la - lb)
    print('%-28s T=%-7d maxA=%-8d maxB=%-8d minB=%-8d avgA=%-9.0f avgB=%-9.0f maxQnHex=%d'
          % (n, T, mla, mlb, mn_lb, sla / max(k, 1), slb / max(k, 1), mx_qn))
