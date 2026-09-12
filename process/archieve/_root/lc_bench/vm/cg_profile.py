#!/usr/bin/env python3
"""callgrind 剖析：对指定题目的若干用例统计指令数与函数级热点。

指令数是确定性的 —— CPU 被抢占、频率漂移都不影响结果，
所以这是「不能测速的夜晚」唯一可信的优化指标。

用法:
    python3 cg_profile.py <add|div|mul> <binary> <tag> [case1 case2 ...]
    不给 case 则用内置的代表性集合。
"""
import re
import subprocess
import sys
from pathlib import Path

LCP = Path('/home/azzr/lcp/big_integer')
PROB = {
    'add': 'addition_of_big_integers',
    'div': 'division_of_big_integers',
    'mul': 'multiplication_of_big_integers',
}
DEFAULT_CASES = {
    'add': ['small_00', 'max_max_00', 'medium_00', 'carry_chain_01', 'large_small_00'],
    'div': ['small_00', 'max_00', 'medium_00', 'burnikel_ziegler_bound_00',
            'length_ratio_integer_00', 'r_nearly_zero_00'],
    'mul': ['small_00', 'max_max_00', 'large_small_00'],
}


def parse_totals(path):
    """从 callgrind 输出文件里读 summary/totals 行。"""
    total = None
    with open(path) as f:
        for ln in f:
            if ln.startswith('summary:') or ln.startswith('totals:'):
                total = int(ln.split(':', 1)[1].strip().split()[0])
    return total


def main():
    key, binary, tag = sys.argv[1], Path(sys.argv[2]).resolve(), sys.argv[3]
    cases = sys.argv[4:] or DEFAULT_CASES[key]
    root = Path(f'/home/azzr/{key}bench')
    cg = root / 'cg'
    cg.mkdir(exist_ok=True)
    indir = LCP / PROB[key] / 'in'

    results = []
    for c in cases:
        out = cg / f'cg_{tag}_{c}.out'
        with open(indir / f'{c}.in', 'rb') as fin:
            subprocess.run(
                ['valgrind', '--tool=callgrind', f'--callgrind-out-file={out}',
                 '--cache-sim=no', '--branch-sim=no', str(binary)],
                stdin=fin, stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, check=False)
        n = parse_totals(out)
        results.append((c, n))
        print(f'  {c:32s} {n:>15,} insn', flush=True)

    print(f'\n=== {tag} 指令数汇总 ({key.upper()}) ===')
    mx = max(n for _, n in results if n) or 1
    for c, n in sorted(results, key=lambda x: -(x[1] or 0)):
        bar = '#' * int(40 * (n or 0) / mx)
        print(f'{c:32s} {n:>15,}  {(n or 0)/mx:5.2f}x  {bar}')


if __name__ == '__main__':
    main()
