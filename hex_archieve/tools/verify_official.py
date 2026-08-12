#!/usr/bin/env python3
"""用官方 hash.json 的 out 哈希直接验证一个解的正确性（不依赖任何 gold 二进制）。

用法:
    python3 verify_official.py <add|div|mul> <binary> [--timeout SEC]

原理:
    Library Checker 的 hash.json 记录了每个用例 in/out 的 sha256。
    我们跑自己的程序生成 out，逐字节哈希比对 —— 这是判题机同款的黄金标准。
注意:
    LC 的 checker 对本题是逐字节比较（无 special judge），所以哈希相等 == AC。
"""
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

LCP = Path('/home/azzr/lcp/big_integer')
PROB = {
    'add': 'addition_of_big_integers',
    'div': 'division_of_big_integers',
    'mul': 'multiplication_of_big_integers',
}


def sha_bytes(b):
    return hashlib.sha256(b).hexdigest()


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    key = sys.argv[1]
    binary = Path(sys.argv[2]).resolve()
    timeout = 120.0
    if '--timeout' in sys.argv:
        timeout = float(sys.argv[sys.argv.index('--timeout') + 1])

    root = LCP / PROB[key]
    hashes = json.load(open(root / 'hash.json'))
    indir = root / 'in'

    cases = sorted(p.stem for p in indir.glob('*.in'))
    ok = bad = miss = 0
    worst = (0.0, '')
    print(f'=== verify {binary.name} on {key.upper()} ({len(cases)} 例) ===')
    for name in cases:
        want = hashes.get(name + '.out')
        if want is None:
            print(f'  [skip] {name}: hash.json 无 out 条目')
            miss += 1
            continue
        with open(indir / f'{name}.in', 'rb') as f:
            t0 = time.perf_counter()
            try:
                r = subprocess.run([str(binary)], stdin=f, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL, timeout=timeout)
            except subprocess.TimeoutExpired:
                print(f'  [TLE ] {name}')
                bad += 1
                continue
            dt = time.perf_counter() - t0
        if r.returncode != 0:
            print(f'  [RE  ] {name}: exit={r.returncode}')
            bad += 1
            continue
        got = sha_bytes(r.stdout)
        if got == want:
            ok += 1
            if dt > worst[0]:
                worst = (dt, name)
        else:
            bad += 1
            print(f'  [WA  ] {name}: got={got[:16]} want={want[:16]} '
                  f'bytes={len(r.stdout)}')
    print(f'--- OK={ok}  BAD={bad}  SKIP={miss} ---')
    # 时间仅作诊断，不用于任何比较判断
    print(f'(诊断用，勿据此比较) 最慢用例: {worst[1]}')
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
