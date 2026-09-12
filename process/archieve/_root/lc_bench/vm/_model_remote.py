
import subprocess, sys
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
for f in sorted(IN.glob('*.in')):
    with open(f, 'rb') as fh:
        p = subprocess.run(['/home/azzr/divbench/bin/sh0'], stdin=fh,
                           capture_output=True, timeout=900)
    for l in p.stderr.decode('utf-8', 'replace').splitlines():
        if '[mushape]' in l:
            print(f.stem, l.replace('[mushape] ', ''))
