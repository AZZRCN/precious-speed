
import re, subprocess, sys
from collections import defaultdict
from pathlib import Path
IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
BIN = Path('/home/azzr/divbench/bin/p_d13')
for CASE in sys.argv[1:]:
    pf = Path('/home/azzr/divbench/prof_detail.log')
    if pf.exists():
        pf.unlink()
    with open(IN / f'{CASE}.in', 'rb') as f:
        p = subprocess.run([str(BIN)], stdin=f, capture_output=True, timeout=900,
                           cwd='/home/azzr/divbench')
    err = p.stderr.decode('utf-8', 'replace')
    if pf.exists():
        err += pf.read_text(errors='replace')
    lines = [l for l in err.splitlines() if '[prof]' in l]
    agg = defaultdict(lambda: [0.0, 0])
    for l in lines:
        m = re.search(r'\[prof\]\s+(.*?):\s+([\d.]+)\s*ms', l)
        if not m:
            continue
        label = re.sub(r'block \d+', 'block N', m.group(1))
        label = re.sub(r'\(.*?\)', '', label).strip()
        agg[label][0] += float(m.group(2))
        agg[label][1] += 1
    tot = sum(v[0] for v in agg.values())
    print(f'\n########## {CASE}   prof lines={len(lines)}  sum={tot:.2f} ms ##########')
    print(f'{"label":<52}{"ms":>10}{"n":>8}{"pct":>8}')
    for k, (ms, n) in sorted(agg.items(), key=lambda kv: -kv[1][0])[:18]:
        print(f'{k[:52]:<52}{ms:10.2f}{n:8d}{(ms/tot*100 if tot else 0):7.1f}%')
    print('--- raw samples ---')
    for l in lines[:12]:
        print('  ' + l.strip()[:145])
