#!/usr/bin/env python3
"""D8 失败根因隔离: 判断错误来自 (a) mu_in 选小 还是 (b) cyclic 循环卷积路径。

变体:
  D8            = 现状(cost-model 选 in, cyclic 开)
  D8_nocyc      = D8 + -DDISABLE_2NXN_CYCLIC (in 仍选小, cyclic 全关)
  D4_nocyc      = D4 + -DDISABLE_2NXN_CYCLIC (对照: 关 cyclic 不该引入错误)
对照黄金实现 div_orig, 跑失败种子集合。
"""
import subprocess
import sys
from pathlib import Path

SRCDIR = Path('/home/azzr/divbench/src')
BINDIR = Path('/home/azzr/divbench/bin')
GEN_DIR = Path('/home/azzr/lcgen/division_of_big_integers/gen')
COMMON = '/home/azzr/lcgen/common'
FLAGS = '-O2 -std=c++23 -march=x86-64-v3'
ORIG = BINDIR / 'div_orig'

VARIANTS = {
    'div_D8_nocyc': ('div_D8.cpp', '-DDISABLE_2NXN_CYCLIC'),
    'div_D4_nocyc': ('div_D4.cpp', '-DDISABLE_2NXN_CYCLIC'),
}

# 失败集合 + 若干对照
CASES = {
    'burnikel_ziegler_bound': [1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 21, 24, 2, 3],
    'medium': [14, 1, 2, 40],
    'r_nearly_zero': [16, 1, 80],
    'length_ratio_integer': [1, 24, 48],
}


def sh(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def build(name, src, extra):
    r = sh(f'g++ {FLAGS} {extra} -o {BINDIR}/{name} {SRCDIR}/{src} 2>&1')
    if r.returncode != 0:
        print(f'BUILD FAIL {name}:\n{r.stdout[-3000:]}', flush=True)
        return False
    print(f'build {name} OK', flush=True)
    return True


def run(binpath, infile):
    with open(infile, 'rb') as f:
        p = subprocess.run([str(binpath)], stdin=f, capture_output=True, timeout=300)
    return p.stdout, p.returncode


def main():
    gens = {}
    for g in CASES:
        out = f'/tmp/gen_{g}'
        r = sh(f'g++ -O2 -std=c++17 -I {COMMON} {GEN_DIR}/{g}.cpp -o {out}')
        if r.returncode == 0:
            gens[g] = out
        else:
            print(f'GENFAIL {g}: {r.stderr[:200]}', flush=True)

    bins = ['div_D8']
    for name, (src, extra) in VARIANTS.items():
        if build(name, src, extra):
            bins.append(name)

    print('\n{:28s} {:>5s}  '.format('case', 'seed') + '  '.join(f'{b:>14s}' for b in bins),
          flush=True)
    summary = {b: [0, 0] for b in bins}  # [total, bad]
    for g, seeds in CASES.items():
        if g not in gens:
            continue
        for seed in seeds:
            inf = f'/tmp/dg_{g}_{seed}.in'
            with open(inf, 'wb') as f:
                subprocess.run([gens[g], str(seed)], stdout=f)
            ref, _ = run(ORIG, inf)
            marks = []
            for b in bins:
                got, rc = run(BINDIR / b, inf)
                ok = (rc == 0 and got == ref)
                summary[b][0] += 1
                if not ok:
                    summary[b][1] += 1
                marks.append('OK' if ok else 'BAD')
            print('{:28s} {:>5d}  '.format(g, seed) + '  '.join(f'{m:>14s}' for m in marks),
                  flush=True)
            Path(inf).unlink(missing_ok=True)

    print('\n=== 汇总 ===')
    for b in bins:
        t, bad = summary[b]
        print(f'  {b:16s} {t:>3d} 例, bad={bad}')

    # 官方 26 例
    for b in bins:
        r = subprocess.run(['python3', '/home/azzr/verify_official.py', 'div', str(BINDIR / b)],
                           capture_output=True, text=True, timeout=2400)
        line = [l for l in (r.stdout + r.stderr).splitlines() if 'OK=' in l]
        wa = [l.strip() for l in (r.stdout + r.stderr).splitlines() if '[WA' in l]
        print(f'  官方26 {b:16s}: {line[-1] if line else "?"}')
        for w in wa[:4]:
            print(f'      {w}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
