#!/usr/bin/env python3
"""D49 正确性验证 (VM 版): 官方 26 例哈希 + 对黄金实现 div_orig 的定向/生成器压测。

与 d8_verify.py 同构, 仅:
  * NAME 默认 div_D49
  * GEN_DIR / COMMON 指向本机 VM 实际路径 (d8 旧路径 big_integer/ 已失效)
黄金 = /home/azzr/divbench/bin/div_orig (官方解, 权威).
"""
import random
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.set_int_max_str_digits(100_000_000)

NAME = sys.argv[1] if len(sys.argv) > 1 else 'div_D49'
SRC = Path(f'/home/azzr/divbench/src/{NAME}.cpp')
BIN = Path(f'/home/azzr/divbench/bin/{NAME}')
ORIG = Path('/home/azzr/divbench/bin/div_orig')
GEN_DIR = Path('/home/azzr/lcgen/division_of_big_integers/gen')
COMMON = '/home/azzr/lcgen/common'
FLAGS = '-O2 -std=c++23 -march=x86-64-v3'

GENS = {
    'a_max_b_random': 24,
    'length_ratio_integer': 48,
    'r_nearly_zero': 80,
    'medium': 40,
    'large': 24,
    'max': 24,
    'small': 40,
    'burnikel_ziegler_bound': 24,
}


def run(binpath, infile):
    with open(infile, 'rb') as f:
        p = subprocess.run([str(binpath)], stdin=f, capture_output=True, timeout=300)
    return p.stdout, p.returncode


def build():
    r = subprocess.run(f'g++ {FLAGS} -o {BIN} {SRC} 2>&1', shell=True,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('BUILD FAIL:\n' + r.stdout[-4000:])
        return False
    print(f'build {NAME} OK', flush=True)
    return True


def official():
    r = subprocess.run(['python3', '/home/azzr/verify_official.py', 'div', str(BIN)],
                       capture_output=True, text=True, timeout=2400)
    out = r.stdout + r.stderr
    print(out[-2500:], flush=True)
    up = out.upper()
    ok = ('BAD=0 ' in up or up.rstrip().endswith('BAD=0')) or ('BAD=0' in up.replace(' ', ''))
    bad_marks = ('[WA' in up) or ('MISMATCH' in up) or ('FAIL' in up) or ('TLE' in up) or ('RE ' in up)
    m = re.search(r'BAD=(\d+)', up)
    if m:
        ok = (int(m.group(1)) == 0)
    return bool(ok) and not bad_marks


def stress_official():
    bins = {}
    for g in GENS:
        out = f'/tmp/gen_{g}'
        r = subprocess.run(['g++', '-O2', '-std=c++17', '-I', COMMON,
                            str(GEN_DIR / f'{g}.cpp'), '-o', out],
                           capture_output=True, text=True)
        if r.returncode == 0:
            bins[g] = out
        else:
            print(f'COMPILE_FAIL {g}: {r.stderr[:300]}', flush=True)
    total = bad = 0
    for g, n in GENS.items():
        if g not in bins:
            continue
        gbad = 0
        for seed in range(1, n + 1):
            inf = f'/tmp/s49_{g}_{seed}.in'
            with open(inf, 'wb') as f:
                subprocess.run([bins[g], str(seed)], stdout=f)
            a, r1 = run(ORIG, inf)
            b, r2 = run(BIN, inf)
            total += 1
            if r1 != 0 or r2 != 0 or a != b:
                bad += 1
                gbad += 1
                print(f'  !! MISMATCH {g} seed={seed} rc={r1}/{r2}', flush=True)
                try:
                    shutil.move(inf, f'/home/azzr/divbench/logs/bad_d49_{g}_{seed}.in')
                except Exception as e:
                    print(f'     (save failed: {e})', flush=True)
            else:
                Path(inf).unlink(missing_ok=True)
        print(f'  {g:26s} {n:>4d} 例  bad={gbad}', flush=True)
    print(f'官方生成器压测: {total} 例, 不一致 {bad}', flush=True)
    return bad == 0


def gen_case(rng, pairs):
    lines = [str(len(pairs))]
    for la, lb in pairs:
        a = rng.randrange(10 ** (la - 1), 10 ** la) if la > 0 else 0
        b = rng.randrange(10 ** (lb - 1), 10 ** lb) if lb > 0 else 1
        if b == 0:
            b = 1
        if rng.random() < 0.5:
            a = -a
        if rng.random() < 0.5:
            b = -b
        lines.append(f'{a} {b}')
    return ('\n'.join(lines) + '\n').encode()


def stress_targeted():
    rng = random.Random(20260803)
    shapes = []
    for l2 in [65, 70, 80, 100, 128, 129, 150, 200, 256, 257, 300, 400, 512, 600]:
        for prod in [5000, 15000, 19000, 20000, 20001, 25000, 40000, 100000]:
            qn = max(1, prod // l2)
            shapes.append((l2, qn))
    for l2 in [200, 500, 1000, 2000, 5000, 9000, 12000]:
        for k in [1, 2, 3, 4, 6, 9, 13, 20, 33]:
            shapes.append((l2, l2 * k + rng.randrange(-3, 4)))
    for base in [1024, 2048, 4096, 8192, 16384]:
        for d in [-2, -1, 0, 1, 2]:
            shapes.append((base + d, 2 * base + d))
            shapes.append((base + d, base // 2 + d))
    shapes = [(a, b) for a, b in shapes if a >= 1 and b >= 1]

    total = bad = 0
    B = 4
    batch, pairs = 0, []
    for l2, qn in shapes:
        la, lb = (l2 + qn) * B, l2 * B
        pairs.append((la, lb))
        if len(pairs) >= 12:
            batch += 1
            inf = f'/tmp/t49d_{batch}.in'
            Path(inf).write_bytes(gen_case(rng, pairs))
            a, r1 = run(ORIG, inf)
            b, r2 = run(BIN, inf)
            total += len(pairs)
            if r1 != 0 or r2 != 0 or a != b:
                bad += len(pairs)
                print(f'  !! MISMATCH targeted batch={batch} rc={r1}/{r2}', flush=True)
                try:
                    shutil.move(inf, f'/home/azzr/divbench/logs/bad_t49d_{batch}.in')
                except Exception as e:
                    print(f'     (save failed: {e})', flush=True)
            else:
                Path(inf).unlink(missing_ok=True)
            pairs = []
    edge = ['8', '0 7', '7 8', '-7 8', '123456789 123456789',
            f'{10**4000} {10**2000}', f'{-(10**4000)} {10**2000}',
            f'{10**4000} {-(10**2000)}', f'{(10**2000) * (10**2000)} {10**2000}']
    inf = '/tmp/t49d_edge.in'
    Path(inf).write_text('\n'.join(edge) + '\n')
    a, r1 = run(ORIG, inf)
    b, r2 = run(BIN, inf)
    total += 8
    if r1 != 0 or r2 != 0 or a != b:
        bad += 8
        print('  !! MISMATCH edge cases', flush=True)
    print(f'定向形状压测: {total} 例, 不一致 {bad}', flush=True)
    return bad == 0


def main():
    Path('/home/azzr/divbench/logs').mkdir(parents=True, exist_ok=True)
    if not build():
        return 1
    ok1 = official()
    print(f'--- 官方 26 例哈希: {"PASS" if ok1 else "FAIL"} ---\n', flush=True)
    ok2 = stress_targeted()
    ok3 = stress_official()
    print(f'\n=== 汇总: 官方哈希={ok1}  定向={ok2}  生成器={ok3} ===')
    return 0 if (ok1 and ok2 and ok3) else 1


if __name__ == '__main__':
    sys.exit(main())
