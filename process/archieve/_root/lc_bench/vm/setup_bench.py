#!/usr/bin/env python3
"""在 VM 上搭建 add/div/mul 三题的基准工作区，用官方 LF 用例（软链接，避免复制）。"""
import os
import subprocess
import sys
from pathlib import Path

LCP = Path('/home/azzr/lcp/big_integer')
PROB = {
    'add': 'addition_of_big_integers',
    'div': 'division_of_big_integers',
    'mul': 'multiplication_of_big_integers',
}


def main():
    for key, prob in PROB.items():
        root = Path(f'/home/azzr/{key}bench')
        for sub in ('src', 'bin', 'cg', 'logs'):
            (root / sub).mkdir(parents=True, exist_ok=True)

        src_in = LCP / prob / 'in'
        link = root / 'cases_lf'
        if link.is_symlink() or link.exists():
            if link.is_symlink():
                link.unlink()
            else:
                print(f'  [warn] {link} 存在且不是软链接，跳过')
                continue
        os.symlink(src_in, link)

        n = len(list(src_in.glob('*.in')))
        # 最大用例
        biggest = max(src_in.glob('*.in'), key=lambda p: p.stat().st_size)
        print(f'{key.upper():4s} {root}  cases_lf -> {src_in}  ({n} 例, 最大 '
              f'{biggest.name} {biggest.stat().st_size/1e6:.1f} MB)')

    print('\n=== 各题用例规模 (前 6 大) ===')
    for key, prob in PROB.items():
        src_in = LCP / prob / 'in'
        items = sorted(src_in.glob('*.in'), key=lambda p: -p.stat().st_size)[:6]
        print(f'--- {key.upper()} ---')
        for p in items:
            print(f'   {p.stat().st_size/1e6:8.2f} MB  {p.name}')


if __name__ == '__main__':
    main()
