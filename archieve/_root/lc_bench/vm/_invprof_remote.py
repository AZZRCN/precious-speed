
import re, subprocess, sys
from collections import defaultdict
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
for CASE in sys.argv[1:]:
    print('\n############### %s ###############' % CASE)
    for d in [0, 1]:
        pf = Path('/home/azzr/divbench/prof_detail.log')
        if pf.exists():
            pf.unlink()
        with open(IN / (CASE + '.in'), 'rb') as f:
            p = subprocess.run(['/home/azzr/divbench/bin/ip%d' % d], stdin=f,
                               capture_output=True, timeout=900,
                               cwd='/home/azzr/divbench')
        err = p.stderr.decode('utf-8', 'replace')
        prof = pf.read_text(errors='replace') if pf.exists() else ''
        agg = defaultdict(lambda: [0.0, 0])
        for l in prof.splitlines():
            m = re.search(r'\[prof\]\s+(.*?):\s+([\d.]+)\s*ms', l)
            if not m:
                continue
            lab = re.sub(r'block \d+', 'block N', m.group(1))
            lab = re.sub(r'\(.*?\)', '', lab).strip()
            agg[lab][0] += float(m.group(2))
            agg[lab][1] += 1
        print('  ===== delta=%d =====' % d)
        for k in ('absDivMu: total', 'absDivMu: blocks loop',
                  'absDivMu: absInvNewton+prepareDFT',
                  'block N: fftMulPre #1', 'block N: fftMulModBm1Pre #2'):
            if k in agg:
                print('    %-40s %9.3f ms  n=%d' % (k, agg[k][0], agg[k][1]))
        for l in err.splitlines():
            if '[invprof]' in l or '[invph]' in l:
                print('    ' + l.replace('[invprof] ', 'INV ').replace('[invph] ', 'PH  '))
