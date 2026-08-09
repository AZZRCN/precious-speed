#!/usr/bin/env python3
"""D6 标定 v2: 高效标定 cyclic 模式下 in 的全局最小安全下界。

策略:
  - 同一 calib 二进制 (运行时 env 控制 in/cyclic, 只编译一次)。
  - 从大到小测 in in {256,192,160,128,96,80,64}, cyclic=1。
  - 每档跑精简集: medium 全 40 种子 (D5 翻车区, 必全扫) + 其余 7 类各 16 种子
    + 全定向形状 + edge cases。某 in 不安全 (任一生成器失配) 即停, 其上最近
    的 SAFE in 即为最小安全下界候选 floor。
  - 单调性假设: in 越小 -> 逆精度越低 -> 越不安全。首个 UNSAFE 之上即为安全区。
  - 阶段2: 对 floor 跑全量 304 生成器 + 224 定向确认 (堵漏网), 若仍不安全则 floor 上调一档重确认。
输出: ~/divbench/sweep2_floor.txt
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
# 从大到小
INS = [256, 192, 160, 128, 96, 80, 64]
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
    """per: 每类跑前 per 个种子; medium 始终全 40。返回 (total,bad)。"""
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
                shutil.move(str(inf), f'/home/azzr/divbench/logs/bad_o6_{g}_{seed}.in')
                break  # 该生成器已不安全, 提前结束本类
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
                shutil.move(str(inf), f'/home/azzr/divbench/logs/bad_t6_{batch}.in')
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
    print('\n===== 阶段1: 从大到小精简标定 =====', flush=True)
    for fi in INS:
        env = dict(os.environ)
        env['DIV_FORCE_IN'] = str(fi)
        print(f'\n----- forced_in={fi} (real cyclic/linear path) -----', flush=True)
        t1, b1 = stress_official(env, PER)
        t2, b2 = stress_targeted(env)
        bad = b1 + b2
        res[fi] = bad
        tag = 'SAFE' if bad == 0 else 'UNSAFE'
        print(f'  >> 生成器 bad={b1}/{t1}  定向 bad={b2}/{t2}  => {tag}', flush=True)
        if bad == 0:
            last_safe = fi
        else:
            # 首个 UNSAFE: 其上最近 SAFE 即候选下界, 停
            floor = last_safe
            print(f'  >> 首个 UNSAFE at in={fi}, 候选下界 floor={floor}', flush=True)
            break
    if floor is None:
        # 全部 SAFE (到 64)
        floor = 64 if res.get(64, 1) == 0 else last_safe
        print(f'  >> 全部 SAFE, 候选下界 floor={floor}', flush=True)

    # 阶段2: 全量确认 floor
    print(f'\n===== 阶段2: 全量确认 in={floor} cyclic=1 =====', flush=True)
    env = dict(os.environ)
    env['DIV_FORCE_IN'] = str(floor)
    t1, b1 = stress_official(env, 99999)  # 全 304
    t2, b2 = stress_targeted(env)
    confirm_bad = b1 + b2
    print(f'  >> 全量: 生成器 bad={b1}/{t1}  定向 bad={b2}/{t2}  => {"CONFIRMED SAFE" if confirm_bad==0 else "LEAK! 需上调"}', flush=True)
    if confirm_bad != 0:
        # 上调一档
        idx = INS.index(floor)
        if idx + 1 < len(INS):
            nf = INS[idx + 1]
            print(f'  >> 上调 floor {floor} -> {nf}, 重确认', flush=True)
            env['DIV_FORCE_IN'] = str(nf)
            t1, b1 = stress_official(env, 99999)
            t2, b2 = stress_targeted(env)
            if b1 + b2 == 0:
                floor = nf
                print(f'  >> 上调后 CONFIRMED SAFE, 最终 floor={floor}', flush=True)
            else:
                print(f'  >> 上调后仍 UNSAFE, floor={nf} 仍泄露, 需人工排查', flush=True)

    with open('/home/azzr/divbench/sweep2_floor.txt', 'w') as f:
        f.write(f'FLOOR_CYCLIC={floor}\n')
        for fi in INS:
            f.write(f'in={fi} bad={res.get(fi, "n/a")}\n')
        f.write(f'CONFIRM bad={confirm_bad}\n')
    print(f'\n===== 结论: cyclic 最小安全 in 下界 = {floor} =====', flush=True)
    return 0


if __name__ == '__main__':
    sys.exit(main())
