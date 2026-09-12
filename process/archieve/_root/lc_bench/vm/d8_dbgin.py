#!/usr/bin/env python3
"""打印 D8 在失败用例上的 in 选择 (natural vs chosen), 定位 cyclic 触发条件差异。"""
import subprocess
import sys
from collections import Counter
from pathlib import Path

SRCDIR = Path('/home/azzr/divbench/src')
BINDIR = Path('/home/azzr/divbench/bin')
GEN_DIR = Path('/home/azzr/lcgen/division_of_big_integers/gen')
COMMON = '/home/azzr/lcgen/common'
FLAGS = '-O2 -std=c++23 -march=x86-64-v3 -DDIV_DEBUG_IN'

CASES = [('burnikel_ziegler_bound', 1), ('burnikel_ziegler_bound', 4),
         ('burnikel_ziegler_bound', 2), ('medium', 14), ('medium', 1),
         ('length_ratio_integer', 24)]


def sh(c):
    return subprocess.run(c, shell=True, capture_output=True, text=True)


def main():
    r = sh(f'g++ {FLAGS} -o {BINDIR}/div_D8_dbg {SRCDIR}/div_D8.cpp 2>&1')
    if r.returncode != 0:
        print(r.stdout[-3000:])
        return 1
    print('build div_D8_dbg OK', flush=True)

    gens = {}
    for g, _ in CASES:
        if g in gens:
            continue
        out = f'/tmp/gen_{g}'
        rr = sh(f'g++ -O2 -std=c++17 -I {COMMON} {GEN_DIR}/{g}.cpp -o {out}')
        gens[g] = out if rr.returncode == 0 else None

    for g, seed in CASES:
        if not gens.get(g):
            continue
        inf = f'/tmp/dbg_{g}_{seed}.in'
        with open(inf, 'wb') as f:
            subprocess.run([gens[g], str(seed)], stdout=f)
        with open(inf, 'rb') as f:
            p = subprocess.run([str(BINDIR / 'div_D8_dbg')], stdin=f,
                               capture_output=True, timeout=300)
        lines = [l for l in p.stderr.decode('utf-8', 'replace').splitlines()
                 if l.startswith('[divdbg]')]
        cnt = Counter(lines)
        print(f'\n### {g} seed={seed}   mu-dispatch 次数={len(lines)}', flush=True)
        changed = [l for l in cnt if 'natural=' in l and
                   l.split('natural=')[1].split()[0] != l.split('chosen=')[1].split()[0]]
        for l, c in cnt.most_common(6):
            mark = '  <== CHANGED' if l in changed else ''
            print(f'   x{c:<4d} {l[9:]}{mark}', flush=True)
        print(f'   其中 in 被改动的形状: {len(changed)} 种', flush=True)
        Path(inf).unlink(missing_ok=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
