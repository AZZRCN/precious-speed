#!/usr/bin/env python3
"""D6 标定: 扫 in 的安全下界。

编译带 env 强制钩子的 calib 二进制 (div_D6_calib.cpp)，对每个 (forced_in, cyclic)
组合跑 304 例官方生成器 + 224 例定向形状, 对黄金实现 div_orig 逐字节比对。
输出每个组合的不一致数, 并给出 cyc=0 / cyc=1 各自的最小安全 in (全局下界)。
"""
import os
import subprocess
import sys
import random
from pathlib import Path
from subprocess import TimeoutExpired

sys.set_int_max_str_digits(100_000_000)

SRC = Path('/home/azzr/divbench/src/div_D6_calib.cpp')
BIN = Path('/home/azzr/divbench/bin/div_calib')
ORIG = Path('/home/azzr/divbench/bin/div_orig')
GEN_DIR = Path('/home/azzr/lcgen/division_of_big_integers/gen')
COMMON = '/home/azzr/lcgen/common'
FLAGS = '-O2 -std=c++23 -march=x86-64-v3'

GENS = {
    'a_max_b_random': 24, 'length_ratio_integer': 48, 'r_nearly_zero': 80,
    'medium': 40, 'large': 24, 'max': 24, 'small': 40, 'burnikel_ziegler_bound': 24,
}

# 线性 (cyclic=0) 的 in>=64 安全性由 Newton 理论 + D4(#390120 75ms) 保证, 不扫。
# 仅标定 cyclic=1 的安全下界 (这是编辑 A 翻车的区域): 覆盖 64..256 各档。
CONFIGS = [(fi, 1) for fi in [64, 80, 96, 128, 160, 192, 256]]

_gen_bins = {}


def build():
    r = subprocess.run(f'g++ {FLAGS} -o {BIN} {SRC} 2>&1', shell=True,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('BUILD FAIL:\n' + r.stdout[-4000:]); return False
    print('build div_calib OK', flush=True); return True


def gen_bins():
    for g in GENS:
        out = f'/tmp/gen_{g}'
        r = subprocess.run(['g++', '-O2', '-std=c++17', '-I', COMMON,
                            str(GEN_DIR / f'{g}.cpp'), '-o', out], capture_output=True, text=True)
        if r.returncode == 0:
            _gen_bins[g] = out
        else:
            print(f'COMPILE_FAIL {g}: {r.stderr[:200]}', flush=True)


def run_env(binpath, infile, env):
    try:
        with open(infile, 'rb') as f:
            p = subprocess.run([str(binpath)], stdin=f, capture_output=True, timeout=120, env=env)
        return p.stdout, p.returncode
    except TimeoutExpired:
        return b'', -1


def stress_official(env):
    total = bad = 0
    for g, n in GENS.items():
        if g not in _gen_bins:
            continue
        gbad = 0
        for seed in range(1, n + 1):
            inf = f'/tmp/s6_{g}_{seed}.in'
            with open(inf, 'wb') as f:
                subprocess.run([_gen_bins[g], str(seed)], stdout=f)
            a, r1 = run_env(ORIG, inf, None)
            b, r2 = run_env(BIN, inf, env)
            total += 1
            if r1 != 0 or r2 != 0 or a != b:
                bad += 1; gbad += 1
                print(f'  !! MISMATCH {g} seed={seed} rc={r1}/{r2}', flush=True)
                Path(inf).rename(f'/home/azzr/divbench/logs/bad_o6_{g}_{seed}.in')
                break  # 该组合对本生成器已不安全, 提前结束
            Path(inf).unlink(missing_ok=True)
        print(f'  {g:26s} {n:>4d} 例  bad={gbad}', flush=True)
    return total, bad


def gen_case(rng, pairs):
    lines = [str(len(pairs))]
    for la, lb in pairs:
        a = rng.randrange(10 ** (la - 1), 10 ** la) if la > 0 else 0
        b = rng.randrange(10 ** (lb - 1), 10 ** lb) if lb > 0 else 1
        if b == 0: b = 1
        if rng.random() < 0.5: a = -a
        if rng.random() < 0.5: b = -b
        lines.append(f'{a} {b}')
    return ('\n'.join(lines) + '\n').encode()


def stress_targeted(env):
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
            inf = f'/tmp/t6_{batch}.in'
            Path(inf).write_bytes(gen_case(rng, pairs))
            a, r1 = run_env(ORIG, inf, None)
            b, r2 = run_env(BIN, inf, env)
            total += len(pairs)
            if r1 != 0 or r2 != 0 or a != b:
                bad += len(pairs)
                print(f'  !! MISMATCH targeted batch={batch} rc={r1}/{r2}', flush=True)
                Path(inf).rename(f'/home/azzr/divbench/logs/bad_t6_{batch}.in')
            else:
                Path(inf).unlink(missing_ok=True)
            pairs = []
    edge = ['8', '0 7', '7 8', '-7 8', '123456789 123456789',
            f'{10**4000} {10**2000}', f'{-(10**4000)} {10**2000}',
            f'{10**4000} {-(10**2000)}', f'{(10**2000) * (10**2000)} {10**2000}']
    inf = '/tmp/t6_edge.in'
    Path(inf).write_text('\n'.join(edge) + '\n')
    a, r1 = run_env(ORIG, inf, None)
    b, r2 = run_env(BIN, inf, env)
    total += 8
    if r1 != 0 or r2 != 0 or a != b:
        bad += 8
        print('  !! MISMATCH edge cases', flush=True)
    return total, bad


def main():
    Path('/home/azzr/divbench/logs').mkdir(exist_ok=True)
    if not build():
        return 1
    gen_bins()
    results = {}
    for fi, fc in CONFIGS:
        env = dict(os.environ)
        env['DIV_FORCE_IN'] = str(fi)
        env['DIV_FORCE_CYCLIC'] = str(fc)
        print(f'\n===== forced_in={fi} cyclic={fc} =====', flush=True)
        t1, b1 = stress_official(env)
        t2, b2 = stress_targeted(env)
        bad = b1 + b2
        results[(fi, fc)] = bad
        print(f'  >> 生成器 bad={b1}/{t1}  定向 bad={b2}/{t2}  => {"SAFE" if bad == 0 else "UNSAFE"}',
              flush=True)
    print('\n========== 安全下界汇总 ==========', flush=True)
    for fc in [0, 1]:
        safe = [fi for (fi, c) in results if c == fc and results[(fi, c)] == 0]
        floor = min(safe) if safe else None
        print(f'cyclic={fc}: 安全 in = {sorted(safe)}  -> 最小安全下界 = {floor}', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
