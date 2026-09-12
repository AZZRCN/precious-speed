#!/usr/bin/env python3
"""D5 双编辑二分: 定位 medium seed=14 的不一致来自编辑 A(mu-in 代价模型) 还是
编辑 B(schoolbook 面积阈值)。

变体:
  Aoff: 编译加 -DDISABLE_MU_IN_MODEL  (编辑 A 关, 编辑 B 开)
  Boff: 源码回退 schoolbook 判据为原始边长条件 (编辑 A 开, 编辑 B 关)
  FULL: 原 div_D5 (两编辑都开, 已知 medium seed=14 不一致)

对每个变体跑 medium seed=14, 与 div_orig 逐字节比对。
"""
import subprocess
import sys
from pathlib import Path

sys.set_int_max_str_digits(100_000_000)

SRC = Path('/home/azzr/divbench/src/div_D5.cpp')
ORIG = '/home/azzr/divbench/bin/div_orig'
GEN = '/home/azzr/lcgen/division_of_big_integers/gen/medium.cpp'
COMMON = '/home/azzr/lcgen/common'
FLAGS = '-O2 -std=c++23 -march=x86-64-v3'


def sh(cmd, **kw):
    kw.setdefault('capture_output', True)
    kw.setdefault('text', True)
    if 'stdout' in kw or 'stderr' in kw:
        kw.pop('capture_output', None)
    r = subprocess.run(cmd, shell=isinstance(cmd, str), **kw)
    return r


def build_variant(tag, extra_flags='', revert_b=False):
    base = SRC.read_text()
    if revert_b:
        old = ('                if (len2 <= 64 || (len1 - len2) <= 64\n'
               '                    || uint64_t(len1 - len2) * uint64_t(len2) <= uint64_t(DIV_SB_LIMIT))')
        new = '                if (len2 <= 64 || (len1 - len2) <= 64)'
        assert old in base, 'revert_b pattern not found'
        base = base.replace(old, new)
    tmp = f'/tmp/div_D5_{tag}.cpp'
    Path(tmp).write_text(base)
    out = f'/home/azzr/divbench/bin/div_D5_{tag}'
    r = sh(f'g++ {FLAGS} {extra_flags} -o {out} {tmp} 2>&1')
    if r.returncode != 0:
        print(f'[build {tag}] FAIL:\n' + r.stdout[-2000:])
        return None
    print(f'[build {tag}] OK -> {out}', flush=True)
    return out


def gen_input(seed, inf):
    gen = '/tmp/gen_medium_bisect'
    sh(f'g++ -O2 -std=c++17 -I {COMMON} {GEN} -o {gen}')
    with open(inf, 'wb') as f:
        sh([gen, str(seed)], stdout=f)


def run(binpath, inf):
    with open(inf, 'rb') as f:
        p = subprocess.run([binpath], stdin=f, capture_output=True, timeout=300)
    return p.stdout, p.returncode


def first_diff(a, b):
    la, lb = a.split(b'\n'), b.split(b'\n')
    for i, (x, y) in enumerate(zip(la, lb)):
        if x != y:
            return i, x, y
    if len(la) != len(lb):
        return min(len(la), len(lb)), b'<TRUNC>', b'<TRUNC>'
    return None, None, None


def main():
    inf = '/tmp/medium_14.in'
    gen_input(14, inf)

    o, rc0 = run(ORIG, inf)
    print(f'div_orig rc={rc0} outlen={len(o)}', flush=True)

    variants = {
        'FULL': (None, ''),
        'Aoff': (None, '-DDISABLE_MU_IN_MODEL'),
        'Boff': (True, ''),
    }
    for tag, (rb, flags) in variants.items():
        binp = build_variant(tag, extra_flags=flags, revert_b=bool(rb)) if tag != 'FULL' \
            else '/home/azzr/divbench/bin/div_D5'
        if binp is None:
            continue
        b, rc = run(binp, inf)
        if rc != 0 or b != o:
            idx, x, y = first_diff(o, b)
            print(f'[{tag}] MISMATCH rc={rc} first_diff_line={idx}', flush=True)
            if idx is not None:
                print(f'   orig: {x[:120]}', flush=True)
                print(f'   {tag}: {y[:120]}', flush=True)
        else:
            print(f'[{tag}] MATCH div_orig', flush=True)

    # 额外: 打印 medium seed=14 输入的形状(前若干对 A,B 的十进制位数)
    txt = Path(inf).read_text().split('\n')
    n = int(txt[0])
    print(f'\nmedium seed=14: {n} 对', flush=True)
    for i in range(1, min(n, 6) + 1):
        ab = txt[i].split()
        if len(ab) == 2:
            print(f'  pair#{i}: |A|={len(ab[0])} |B|={len(ab[1])} '
                  f'Aneg={ab[0].startswith("-")} Bneg={ab[1].startswith("-")}', flush=True)


if __name__ == '__main__':
    main()
