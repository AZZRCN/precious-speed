
import subprocess, sys
from collections import Counter
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
DELTAS = [0, 1, 2]
for CASE in sys.argv[1:]:
    print('\n########## %s ##########' % CASE)
    for d in DELTAS:
        with open(IN / (CASE + '.in'), 'rb') as f:
            p = subprocess.run(['/home/azzr/divbench/bin/sh%d' % d], stdin=f,
                               capture_output=True, timeout=900)
        lines = [l for l in p.stderr.decode('utf-8', 'replace').splitlines()
                 if '[mushape]' in l]
        cnt = Counter(lines)
        tot = 0.0
        for l in lines:
            for tok in l.split():
                if tok.startswith('blkW='):
                    tot += float(tok[5:])
        print('  --- delta=%d   calls=%d   sum(blkW)=%.4g' % (d, len(lines), tot))
        for l, n in cnt.most_common(6):
            print('    x%-4d %s' % (n, l.replace('[mushape] ', '')))
