#!/usr/bin/env python3
"""VM 侧: 用 Python 大整数为官方测试点生成黄金输出 .exp (幂等, 已存在则跳过)."""
import sys, os, glob, time

def fmt(v):
    return '0' if v == 0 else format(v, 'X')

def solve(prob, a, b):
    av = int(a, 16); bv = int(b, 16)
    if prob == 'add':
        return fmt(av + bv)
    if prob == 'mul':
        return fmt(av * bv)
    return fmt(av // bv) + ' ' + fmt(av % bv)

def do(prob, indir, force=False):
    for inp in sorted(glob.glob(os.path.join(indir, '*.in'))):
        exp = inp[:-3] + '.exp'
        if os.path.exists(exp) and not force and os.path.getmtime(exp) > os.path.getmtime(inp):
            continue
        t0 = time.time()
        with open(inp) as f:
            data = f.read().split()
        t = int(data[0])
        out = []
        idx = 1
        for _ in range(t):
            a = data[idx]; b = data[idx + 1]; idx += 2
            out.append(solve(prob, a, b))
        with open(exp, 'w') as f:
            f.write('\n'.join(out) + '\n')
        print(f'  {os.path.basename(inp)}: T={t} -> exp ({time.time()-t0:.1f}s)', flush=True)

if __name__ == '__main__':
    prob = sys.argv[1]
    indir = sys.argv[2]
    force = '--force' in sys.argv
    print(f'oracle {prob} {indir}')
    do(prob, indir, force)
    print('ORACLE_DONE')
