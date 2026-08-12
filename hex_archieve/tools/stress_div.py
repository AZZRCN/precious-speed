#!/usr/bin/env python3
"""HEX 除法暴力对拍: 随机 + 边界构造, 与 python int 数学等价对比.

用法: python3 stress_div.py <bin> [rounds] [seed]
每轮生成一个多行 case (T 组), 覆盖 BZ 块边界 / 归一化 / 极端长度比.
"""
import random, subprocess, sys, os

BIN = sys.argv[1]
ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 100
SEED0 = int(sys.argv[3]) if len(sys.argv) > 3 else 20260810

# 关键规模 (limb 数): BZ_CUTOFF=64, BZ_MIN=160
KEY_NB = [1, 2, 3, 4, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129,
          159, 160, 161, 191, 192, 193, 255, 256, 257, 319, 320, 321,
          383, 384, 385, 511, 512, 513, 640, 641, 1024, 1025, 1536, 2048]


def mk_num(nlimb, mode, rnd):
    """构造 nlimb limb 的正整数, 返回 int"""
    if mode == 0:      # 全随机
        v = rnd.getrandbits(64 * nlimb)
    elif mode == 1:    # 最高 limb 最高位=1 (sigma=0)
        v = rnd.getrandbits(64 * nlimb) | (1 << (64 * nlimb - 1))
    elif mode == 2:    # 最高 limb 很小 (sigma 大)
        v = rnd.getrandbits(64 * (nlimb - 1)) | (1 << (64 * (nlimb - 1))) if nlimb > 1 else 1
    elif mode == 3:    # 全 F
        v = (1 << (64 * nlimb)) - 1
    elif mode == 4:    # 2 的幂
        v = 1 << (64 * nlimb - 1 - rnd.randrange(0, 64))
    elif mode == 5:    # 高位全 1, 低位全 0
        k = rnd.randrange(1, 64 * nlimb)
        v = ((1 << k) - 1) << (64 * nlimb - k)
        v &= (1 << (64 * nlimb)) - 1
        v |= 1 << (64 * nlimb - 1)
    else:              # 稀疏
        v = 1 << (64 * nlimb - 1)
        for _ in range(rnd.randrange(1, 8)):
            v |= 1 << rnd.randrange(0, 64 * nlimb - 1)
    return max(v, 1)


def gen_case(rnd):
    lines = []
    exp = []
    T = rnd.randrange(1, 12)
    for _ in range(T):
        nb = rnd.choice(KEY_NB)
        # A 的 limb 数: 覆盖 nb, 2nb, k*nb 整除, 略多略少
        style = rnd.randrange(0, 6)
        if style == 0:
            na = nb
        elif style == 1:
            na = 2 * nb
        elif style == 2:
            na = nb * rnd.randrange(2, 6)          # 整数倍 (t 边界)
        elif style == 3:
            na = nb * rnd.randrange(2, 6) + rnd.randrange(1, max(2, nb))
        elif style == 4:
            na = nb + rnd.randrange(0, 3)
        else:
            na = min(4096, nb * rnd.randrange(1, 40) + 1)
        na = max(na, 1)
        b = mk_num(nb, rnd.randrange(0, 7), rnd)
        a = mk_num(na, rnd.randrange(0, 7), rnd)
        if rnd.randrange(0, 6) == 0:
            a = b * rnd.randrange(0, 1 << 64)      # 整除 / r=0
        if rnd.randrange(0, 8) == 0:
            a = b * mk_num(max(1, na - nb), rnd.randrange(0, 7), rnd) + rnd.randrange(0, b)
        if a < 0: a = -a
        lines.append('%x %x' % (a, b))
        exp.append('%x %x' % (a // b, a % b))
    return ('%d\n' % T) + '\n'.join(lines) + '\n', '\n'.join(exp) + '\n'


def norm(s):
    return '\n'.join(x.strip().lower() for x in s.strip().split('\n'))


bad = 0
for r in range(ROUNDS):
    rnd = random.Random(SEED0 + r)
    inp, exp = gen_case(rnd)
    p = subprocess.run([BIN], input=inp.encode(), capture_output=True, timeout=120)
    got = p.stdout.decode()
    if p.returncode != 0:
        print('ROUND %d: RTE rc=%d  stderr=%s' % (r, p.returncode, p.stderr.decode()[:300]))
        open('/tmp/fail_%d.in' % r, 'w').write(inp)
        bad += 1
        if bad >= 3: break
        continue
    if norm(got) != norm(exp):
        ge = norm(got).split('\n'); ee = norm(exp).split('\n')
        print('ROUND %d: WA  (lines got=%d exp=%d)' % (r, len(ge), len(ee)))
        for i in range(min(len(ge), len(ee))):
            if ge[i] != ee[i]:
                il = inp.split('\n')[i + 1]
                print('  case#%d  A_len=%d B_len=%d' % (i, len(il.split()[0]), len(il.split()[1])))
                print('  got: %s' % ge[i][:160])
                print('  exp: %s' % ee[i][:160])
                break
        open('/tmp/fail_%d.in' % r, 'w').write(inp)
        bad += 1
        if bad >= 3: break
    if (r + 1) % 10 == 0:
        print('  ... %d/%d ok' % (r + 1 - bad, ROUNDS), flush=True)

print('STRESS DONE: rounds=%d  bad=%d  %s' % (ROUNDS, bad, 'ALL OK' if bad == 0 else 'FAILED'))
sys.exit(1 if bad else 0)
