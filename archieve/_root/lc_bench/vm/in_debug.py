#!/usr/bin/env python3
"""聚焦 debug: 单用例 (burnikel_ziegler_bound seed=1) 在不同 forced_in 下的安全性。
确认 absDivMu 是否只对 natural in 安全, 还是对所有够大的 in 都安全。"""
import os, subprocess, sys
from pathlib import Path
from subprocess import TimeoutExpired
sys.set_int_max_str_digits(100_000_000)

BIN = '/home/azzr/divbench/bin/div_calib'
ORIG = '/home/azzr/divbench/bin/div_orig'
GEN_DIR = Path('/home/azzr/lcgen/division_of_big_integers/gen')
COMMON = '/home/azzr/lcgen/common'
GEN = '/tmp/gen_burnikel_ziegler_bound'

def build_gen():
    r = subprocess.run(['g++', '-O2', '-std=c++17', '-I', COMMON,
                        str(GEN_DIR / 'burnikel_ziegler_bound.cpp'), '-o', GEN],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('GEN FAIL', r.stderr[:300]); sys.exit(1)

def run(b, inf, env):
    with open(inf, 'rb') as f:
        p = subprocess.run([b], stdin=f, capture_output=True, timeout=120, env=env)
    return p.stdout, p.returncode

def main():
    build_gen()
    inf = '/tmp/bz1.in'
    subprocess.run([GEN, '1'], stdout=open(inf, 'wb'))
    ref, rc = run(ORIG, inf, None)
    print(f'ref len={len(ref)} rc={rc}', flush=True)
    # 也测 natural (不强制)
    out0, rc0 = run(BIN, inf, None)
    print(f'[natural] match={out0==ref} rc={rc0} len={len(out0)}', flush=True)
    for fi in [48, 64, 96, 128, 160, 192, 256, 384, 512, 768, 1024, 2048, 4096]:
        env = dict(os.environ); env['DIV_FORCE_IN'] = str(fi)
        out, rc = run(BIN, inf, env)
        ok = (rc == 0 and out == ref)
        # 输出前 60 字节差异提示
        diff = ''
        if not ok and out:
            m = 0
            for i in range(min(len(out), len(ref))):
                if out[i] != ref[i]:
                    m = i; break
            diff = f' firstdiff@{m}'
        print(f'in={fi:5d} match={ok} rc={rc} len={len(out)} reflen={len(ref)}{diff}', flush=True)

if __name__ == '__main__':
    main()
