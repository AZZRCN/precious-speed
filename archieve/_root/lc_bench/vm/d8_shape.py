#!/usr/bin/env python3
"""在官方用例上 dump absDivMu 的形状与 in 选择, 并按 FFT 工作量排序找真正的热点形状。"""
import subprocess
import sys
from collections import Counter
from pathlib import Path

IN = Path('/home/azzr/lcp/big_integer/division_of_big_integers/in')
BIN = Path('/home/azzr/divbench/bin/div_D8_dbg')
CASES = ['length_ratio_integer_02', 'length_ratio_integer_00', 'a_max_b_random_02',
         'r_nearly_zero_01', 'burnikel_ziegler_bound_00', 'medium_02']


def ceil2(x):
    n = 1
    while n < x:
        n <<= 1
    return n


def work(len2, qn, in_):
    """估算该形状的 FFT 工作量 (bucket 总和), 与 C++ 端 cost 一致。"""
    recip = ceil2(2 * in_ + 1)
    cm = max(ceil2(len2 + 1), ceil2((len2 + in_) // 2 + 1))
    cyc = cm < in_ + len2
    b2 = cm if cyc else ceil2(len2 + in_)
    blocks = (qn + in_ - 1) // in_
    return recip + blocks * (recip + b2), blocks, recip, b2, cyc


def main():
    for case in CASES:
        inf = IN / f'{case}.in'
        if not inf.exists():
            print(f'skip {case}')
            continue
        with open(inf, 'rb') as f:
            p = subprocess.run([str(BIN)], stdin=f, capture_output=True, timeout=600)
        rows = []
        for l in p.stderr.decode('utf-8', 'replace').splitlines():
            if not l.startswith('[divdbg]'):
                continue
            d = dict(kv.split('=') for kv in l[9:].split())
            rows.append((int(d['len2']), int(d['qn']), int(d['natural']),
                         int(d['chosen']), int(d['cyc'])))
        cnt = Counter(rows)
        tot_nat = tot_new = 0
        scored = []
        for (len2, qn, nat, cho, cyc), c in cnt.items():
            wn, bn, rn_, b2n, cn = work(len2, qn, nat)
            wc, bc, rc_, b2c, cc = work(len2, qn, cho)
            tot_nat += wn * c
            tot_new += wc * c
            scored.append((wn * c, c, len2, qn, nat, cho, wn, wc, bn, bc, rn_, rc_, b2n, b2c))
        scored.sort(reverse=True)
        print(f'\n### {case}: mu 调用 {len(rows)} 次, 形状 {len(cnt)} 种')
        print(f'    总 FFT 工作量: natural={tot_nat:,}  chosen={tot_new:,}  '
              f'比值={tot_new / tot_nat if tot_nat else 0:.4f}')
        print('    热点形状 (按 natural 工作量降序, top5):')
        for s in scored[:5]:
            (_, c, len2, qn, nat, cho, wn, wc, bn, bc, rn_, rc_, b2n, b2c) = s
            print(f'      x{c:<3d} len2={len2:<7d} qn={qn:<7d} '
                  f'nat={nat:<7d}(blk={bn} recip={rn_} b2={b2n} w={wn:,})  '
                  f'-> cho={cho:<7d}(blk={bc} recip={rc_} b2={b2c} w={wc:,})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
