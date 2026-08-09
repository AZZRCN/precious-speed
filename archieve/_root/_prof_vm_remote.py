#!/usr/bin/env python3
import subprocess, os, re
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
CASES = ['a_max_b_random_02', 'r_nearly_zero_01', 'burnikel_ziegler_bound_02', 'length_ratio_integer_03', 'burnikel_ziegler_bound_00', 'large_00', 'small_00', 'max_01']
EXE = '/home/azzr/divbench/bin/d47_tp'
PAT = re.compile(r"read=([\d.]+) parse=([\d.]+) div=([\d.]+) fmt=([\d.]+) write=([\d.]+)\s+total=([\d.]+)")

hdr = '%-28s %7s %7s %8s %7s %7s %9s   rd/ps/div/fmt/wr   nondiv%%' % (
    'case','read','parse','div','fmt','write','TOTAL')
print(hdr, flush=True); print('-'*len(hdr), flush=True)
rows=[]
for c in CASES:
    f = IN + c + '.in'
    if not os.path.exists(f):
        print('%-28s MISSING' % c, flush=True); continue
    best=None
    for _ in range(5):                       # 5 轮取 total 最小, 消抖
        with open(f,'rb') as fi:
            p = subprocess.run([EXE], stdin=fi, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE)
        m = PAT.search(p.stderr.decode('utf-8','replace'))
        if not m: continue
        v=[float(x) for x in m.groups()]
        if best is None or v[5]<best[5]: best=v
    if best is None:
        print('%-28s NOPROF' % c, flush=True); continue
    rd,ps,dv,fm,wr,tot = best
    io = rd+ps+fm+wr
    print('%-28s %7.2f %7.2f %8.2f %7.2f %7.2f %9.2f   %.0f/%.0f/%.0f/%.0f/%.0f   %.1f' % (
        c,rd,ps,dv,fm,wr,tot,
        100*rd/tot,100*ps/tot,100*dv/tot,100*fm/tot,100*wr/tot,100*io/tot), flush=True)
    rows.append((c,rd,ps,dv,fm,wr,tot))
print('-'*len(hdr), flush=True)
HL=('a_max_b_random_02','r_nearly_zero_01','burnikel_ziegler_bound_02')
hl=[r for r in rows if r[0] in HL]
if len(hl)==3:
    print('', flush=True)
    print('[headline 三点 / Linux mmap 真实路径]', flush=True)
    for c,rd,ps,dv,fm,wr,tot in hl:
        print('  %-28s 非除法=%6.2f ms (%4.1f%%)   纯除法=%6.2f ms' % (
            c, rd+ps+fm+wr, 100*(rd+ps+fm+wr)/tot, dv), flush=True)
    mn=min(r[1]+r[2]+r[4]+r[5] for r in hl)
    print('', flush=True)
    print('  => 三点公共 IO/格式化地板 >= %.2f ms (VM Linux)' % mn, flush=True)
print('DONE', flush=True)
