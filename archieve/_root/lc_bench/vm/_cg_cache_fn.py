#!/usr/bin/env python3
import subprocess, re, os
FILES = [('fft3 ', '/tmp/ab_d16_length_ratio_integer_03.out'),
         ('nofft3', '/tmp/ab_d16_nf3_length_ratio_integer_03.out')]
WANT = ['dif3Stage', 'idit3Stage', 'dif<false>', 'idit<false>', 'dif<true>',
        'real_dot_binrev3', 'rdot', 'Float2', 'memset', 'memcpy']

for tag, f in FILES:
    if not os.path.exists(f):
        print('MISSING', f, flush=True); continue
    r = subprocess.run(['callgrind_annotate', '--threshold=99.9',
                        '--show=Ir,D1mr,D1mw,DLmr,DLmw', f],
                       capture_output=True, text=True)
    out = r.stdout
    ev = None
    for line in out.splitlines():
        if line.strip().startswith('Events shown:'):
            ev = line.split(':', 1)[1].split(); break
    if not ev:
        print('NO EVENTS; head of output:', flush=True)
        print('\n'.join(out.splitlines()[:25]), flush=True)
        continue
    iIr = ev.index('Ir'); iD1 = ev.index('D1mr'); iDL = ev.index('DLmr')
    iD1w = ev.index('D1mw') if 'D1mw' in ev else None
    print('=== %s   events=%s' % (tag, ev), flush=True)
    print('  %-24s %13s %11s %9s %8s %8s' % ('fn','Ir','D1mr','DLmr','D1mr/Ir%','est/Ir'), flush=True)
    tot = None
    rows = []
    for line in out.splitlines():
        m = re.match(r'^\s*([\d,]+(?:\s+[\d,]+)*)\s+(\S.*)$', line)
        if not m: continue
        nums = [int(x.replace(',', '')) for x in m.group(1).split()]
        if len(nums) < len(ev): continue
        name = m.group(2).strip()
        rows.append((nums, name))
    agg = {}
    for nums, name in rows:
        fn = name.split(':')[-1].strip()
        for w in WANT:
            if w in fn:
                a = agg.setdefault(w, [0]*len(ev))
                for i, v in enumerate(nums): a[i] += v
                break
    for w in WANT:
        if w not in agg: continue
        a = agg[w]
        Ir = a[iIr] or 1
        d1 = a[iD1] + (a[iD1w] if iD1w is not None else 0)
        dl = a[iDL]
        est = Ir + 5*d1 + 200*dl
        print('  %-24s %13d %11d %9d %7.3f%% %8.3f' % (w, Ir, d1, dl, 100.0*d1/Ir, est/Ir), flush=True)
    print(flush=True)
print('DONE', flush=True)
