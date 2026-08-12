#!/usr/bin/env python3
# AZZRCN
# https://github.com/AZZRCN
"""定向压测 mul_unbal 路径 (v15): 只生成能触发不平衡分块的形状.

触发条件 (v15): mb >= UNBAL_MIN_LIMB(4096) 且 ma >= 2*mb
用法: python3 unbal_stress.py <bin> [rounds] [seed]

覆盖要点:
  * mb 恰在阈值边界 4095/4096/4097
  * ratio 恰在 2 的边界 (ma = 2mb-1 / 2mb / 2mb+1)
  * 块数 = ceil(ma/mb) 的边界 (整除 / 余 1 limb / 余 mb-1 limb)
  * merge_add_b2 的跨块进位: 全 F 乘全 F (最大进位链)
  * 前导零 limb / 单 bit / 符号
"""
import random
import subprocess
import sys

BIN = sys.argv[1]
ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 200
SEED0 = int(sys.argv[3]) if len(sys.argv) > 3 else 20260816

# 题目上限: 每个操作数 <= 1,600,000 hex 字符 = 100,000 limb
MAX_LIMB = 100000
MB = [4095, 4096, 4097, 4100, 5000, 6144, 8191, 8192, 8193,
      10000, 12500, 16384, 20000, 33333, 49999, 50000]
RATIO = [2, 3, 4, 5, 7, 9, 12, 17, 24]


def mk_num(nlimb, mode, rnd):
    nb = 64 * nlimb
    if mode == 0:
        v = rnd.getrandbits(nb)
    elif mode == 1:
        v = rnd.getrandbits(nb) | (1 << (nb - 1))
    elif mode == 2:                       # 全 F: 最长进位链
        v = (1 << nb) - 1
    elif mode == 3:
        v = 1 << (nb - 1)
    elif mode == 4:                       # 高位密集低位稀疏
        v = (1 << (nb - 1)) | rnd.getrandbits(64)
    elif mode == 5:                       # 0xFFFF...0000...FFFF
        k = nb // 3
        v = (((1 << k) - 1) << (nb - k)) | ((1 << k) - 1)
    else:
        v = rnd.getrandbits(nb) | (1 << (nb - 1))
    return max(v, 1)


def fmt(v):
    return ('-%x' % -v) if v < 0 else ('%x' % v)


def gen_case(rnd):
    lines, exp, shapes = [], [], []
    T = rnd.randrange(1, 3)
    for _ in range(T):
        mb = rnd.choice(MB)
        feas = [r for r in RATIO if mb * r <= MAX_LIMB]
        if not feas:
            feas = [2]
        r = rnd.choice(feas)
        ma = mb * r + rnd.choice([-1, 0, 1, mb // 2, mb - 1])
        if ma < 2 * mb:
            ma = 2 * mb
        if ma > MAX_LIMB:
            ma = MAX_LIMB
        a = mk_num(ma, rnd.randrange(0, 7), rnd)
        b = mk_num(mb, rnd.randrange(0, 7), rnd)
        sa = -1 if rnd.randrange(0, 4) == 0 else 1
        sb = -1 if rnd.randrange(0, 4) == 0 else 1
        A, B = sa * a, sb * b
        lines.append('%s %s' % (fmt(A), fmt(B)))
        exp.append(fmt(A * B))
        shapes.append((ma, mb, r))
    return ('%d\n' % T) + '\n'.join(lines) + '\n', '\n'.join(exp) + '\n', shapes


def main():
    bad = 0
    for r in range(ROUNDS):
        rnd = random.Random(SEED0 + r)
        inp, want, shapes = gen_case(rnd)
        p = subprocess.run([BIN], input=inp, capture_output=True, text=True)
        got = p.stdout.strip().upper()
        want = want.strip().upper()
        if got != want.strip():
            bad += 1
            print('BAD round=%d shapes=%s' % (r, shapes))
            for i, (g, w) in enumerate(zip(got.split('\n'), want.strip().split('\n'))):
                if g != w:
                    print('  line %d: len got=%d want=%d' % (i, len(g), len(w)))
                    print('  got  head=%s tail=%s' % (g[:40], g[-40:]))
                    print('  want head=%s tail=%s' % (w[:40], w[-40:]))
                    break
            if bad >= 3:
                print('ABORT after 3 bad')
                return 1
        if (r + 1) % 10 == 0:
            print('  ... %d/%d ok(bad=%d)' % (r + 1, ROUNDS, bad), flush=True)
    print('DONE rounds=%d bad=%d' % (ROUNDS, bad))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
