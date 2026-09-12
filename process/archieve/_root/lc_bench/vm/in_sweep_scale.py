#!/usr/bin/env python3
"""D6 标定 v3: 相对 natural in 的缩放比例下界 (cyclic/linear 真实路径)。

D6 选择器要"缩小 in 降 FFT", 但 absDivMu 对 in 极敏感 (burnikel 类 in 窗口仅 48-64)。
真正的收益在大 len2 瓶颈点 (natural in 很大), 故标定"相对 natural in 的缩放比例"。

用法: DIV_FORCE_IN_SCALE = k (0<k<=1), 钩子把 in 设为 max(1, natural_in*k) clamp len2。
从 k=1.0 往下, 精简集 (medium 全扫 + 其余类 16 种子 + 全定向) 首个 UNSAFE 即停,
其上一档为安全比例下界候选, 再全量 304+224 确认。
输出: ~/divbench/sweep_scale_floor.txt
"""
import os
import subprocess
import sys
import random
import shutil
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
SCALES = [1.0, 0.85, 0.7, 0.6, 0.5, 0.4, 0.33, 0.25, 0.2, 0.15, 0.1]
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


def stress_official(env, per):
    total = bad = 0
    for g, n in GENS.items():
        cnt = n if g == 'medium' else min(n, per)
        gbad = 0
        for seed in range(1, cnt + 1):
            inf = f'/tmp/s6_{g}_{seed}.in'
            with open(inf, 'wb') as f:
                subprocess.run([_gen_bins[g], str(seed)], stdout=f)
            a, r1 = run_env(ORIG, inf, None)
            b, r2 = run_env(BIN, inf, env)
            total += 1
            if r1 != 0 or r2 != 0 or a != b:
                bad += 1; gbad += 1
                print(f'  !! MISMATCH {g} seed={seed} rc={r1}/{r2}', flush=True)
                shutil.move(str(inf), f'/home/azzr/divbench/logs/bad_s6_{g}_{seed}.in')
                break
            Path(inf).unlink(missing_ok=True)
        print(f'  {g:26s} {cnt:>4d} 例  bad={gbad}', flush=True)
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
                shutil.move(str(inf), f'/home/azzr/divbench/logs/bad_st6_{batch}.in')
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
    PER = 16
    floor = None
    last_safe = None
    res = {}
    print('\n===== 阶段1: 比例下界精简标定 (从 k=1.0 往下) =====', flush=True)
    for k in SCALES:
        env = dict(os.environ)
        env['DIV_FORCE_IN_SCALE'] = repr(k)
        print(f'\n----- scale={k} -----', flush=True)
        t1, b1 = stress_official(env, PER)
        t2, b2 = stress_targeted(env)
        bad = b1 + b2
        res[k] = bad
        tag = 'SAFE' if bad == 0 else 'UNSAFE'
        print(f'  >> 生成器 bad={b1}/{t1}  定向 bad={b2}/{t2}  => {tag}', flush=True)
        if bad == 0:
            last_safe = k
        else:
            floor = last_safe
            print(f'  >> 首个 UNSAFE at scale={k}, 候选下界 floor={floor}', flush=True)
            break
    if floor is None:
        floor = 1.0 if res.get(1.0, 1) == 0 else last_safe
        print(f'  >> 全部 SAFE, 候选下界 floor={floor}', flush=True)

    print(f'\n===== 阶段2: 全量确认 scale={floor} =====', flush=True)
    env = dict(os.environ)
    env['DIV_FORCE_IN_SCALE'] = repr(floor)
    t1, b1 = stress_official(env, 99999)
    t2, b2 = stress_targeted(env)
    confirm_bad = b1 + b2
    print(f'  >> 全量: 生成器 bad={b1}/{t1}  定向 bad={b2}/{t2}  => {"CONFIRMED SAFE" if confirm_bad==0 else "LEAK!"}', flush=True)
    if confirm_bad != 0:
        idx = SCALES.index(floor)
        if idx + 1 < len(SCALES):
            nf = SCALES[idx + 1]
            print(f'  >> 上调 floor {floor} -> {nf}, 重确认', flush=True)
            env['DIV_FORCE_IN_SCALE'] = repr(nf)
            t1, b1 = stress_official(env, 99999)
            t2, b2 = stress_targeted(env)
            if b1 + b2 == 0:
                floor = nf
                print(f'  >> 上调后 CONFIRMED SAFE, 最终 floor={floor}', flush=True)
            else:
                print(f'  >> 上调后仍 UNSAFE, floor={nf} 仍泄露', flush=True)

    with open('/home/azzr/divbench/sweep_scale_floor.txt', 'w') as f:
        f.write(f'FLOOR_SCALE={floor}\n')
        for k in SCALES:
            f.write(f'scale={k} bad={res.get(k, "n/a")}\n')
        f.write(f'CONFIRM bad={confirm_bad}\n')
    print(f'\n===== 结论: 相对 natural in 的安全缩放比例下界 = {floor} =====', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
