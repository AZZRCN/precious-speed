#!/usr/bin/env python3
import subprocess, os, re
IN  = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
EXE = '/home/azzr/divbench/bin/d47_g'
CASES = ['r_nearly_zero_01', 'burnikel_ziegler_bound_02', 'a_max_b_random_02']
TOPN = 16

for c in CASES:
    f = IN + c + '.in'
    if not os.path.exists(f):
        print('MISSING', c, flush=True); continue
    outf = '/tmp/annot_%s.out' % c
    cmd = ('valgrind --tool=callgrind --callgrind-out-file=' + outf +
           ' ' + EXE + ' < ' + f + ' > /dev/null 2>/dev/null')
    subprocess.run(cmd, shell=True)
    r = subprocess.run(['callgrind_annotate', '--auto=no', '--threshold=99', outf],
                       capture_output=True, text=True)
    txt = r.stdout
    # 解析总 Ir
    tot = None
    m = re.search(r'([\d,]+)\s+PROGRAM TOTALS', txt)
    if m: tot = int(m.group(1).replace(',',''))
    print('', flush=True)
    print('='*100, flush=True)
    print('### %s   总 Ir = %s' % (c, '{:,}'.format(tot) if tot else '?'), flush=True)
    print('='*100, flush=True)
    started = False; n = 0
    for line in txt.splitlines():
        if re.match(r'^-+$', line.strip()) and not started:
            continue
        if 'file:function' in line or ('Ir' in line and 'file' in line):
            started = True; continue
        if not started: continue
        mm = re.match(r'^\s*([\d,]+)\s+(?:\(\s*[\d.]+%\)\s+)?(.+)$', line)
        if not mm: continue
        ir = int(mm.group(1).replace(',',''))
        nm = mm.group(2).strip()
        if 'PROGRAM TOTALS' in nm: continue
        n += 1
        if n > TOPN: break
        pct = 100.0*ir/tot if tot else 0.0
        # 缩短符号名
        nm = re.sub(r'\(.*?\)', '()', nm)
        if len(nm) > 78: nm = nm[:75]+'...'
        print('  %7.3f%%  %14s  %s' % (pct, '{:,}'.format(ir), nm), flush=True)
    os.unlink(outf)
print('', flush=True)
print('DONE', flush=True)
