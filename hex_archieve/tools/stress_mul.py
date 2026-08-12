#!/usr/bin/env python3
"""HEX 乘法暴力对拍: 随机 + 边界构造, 与 python int 数学等价对比.

用法: python3 stress_mul.py <bin> [rounds] [seed]
覆盖: 128x128 快路径 (la,lb<=32) / mul_bf (mb<=48) / mul_fft
      pick_k 档位边界 / FFT_LEAF_LOG=11 递归边界 / radix-2^2 残层
      split_b2 gather 尾巴 / 符号 / 前导零 / 零值
"""
import random, subprocess, sys

BIN = sys.argv[1]
ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 100
SEED0 = int(sys.argv[3]) if len(sys.argv) > 3 else 20260810

# limb 关键规模: 快路径 32hex=2limb; mul_bf 阈值 mb<=48;
# pick_k 档位在 u=na+nb 处切换: 304,1152,4352,16384,(61440 太大少采)
KEY_NB = [1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33, 47, 48, 49, 50,
          63, 64, 65, 95, 96, 97, 127, 128, 129, 151, 152, 153,
          255, 256, 257, 511, 512, 513, 575, 576, 577,
          1023, 1024, 1025, 2047, 2048, 2049, 2175, 2176, 2177,
          4095, 4096, 4097, 8191, 8192, 8193]
BIG_NB = [304, 305, 576, 1152, 1153, 2176, 4352, 4353, 8192, 16384, 16385]


def mk_num(nlimb, mode, rnd):
    nb = 64 * nlimb
    if mode == 0:
        v = rnd.getrandbits(nb)
    elif mode == 1:
        v = rnd.getrandbits(nb) | (1 << (nb - 1))
    elif mode == 2:
        v = (1 << (nb - 1)) - 1 if nlimb > 1 else 1
    elif mode == 3:
        v = (1 << nb) - 1
    elif mode == 4:
        v = 1 << rnd.randrange(0, nb)
    elif mode == 5:
        k = rnd.randrange(1, nb)
        v = (((1 << k) - 1) << (nb - k)) & ((1 << nb) - 1)
        v |= 1 << (nb - 1)
    elif mode == 6:
        v = 1 << (nb - 1)
        for _ in range(rnd.randrange(1, 8)):
            v |= 1 << rnd.randrange(0, nb - 1)
    elif mode == 7:                       # 交替 0xF0F0...
        v = int('f0' * (nb // 8), 16) if nb >= 8 else 0xf
    else:                                 # 极小值 (前导零 limb)
        v = rnd.getrandbits(min(nb, 40))
    return max(v, 1)


def gen_case(rnd, heavy):
    lines, exp = [], []
    T = rnd.randrange(1, 10)
    for _ in range(T):
        if heavy and rnd.randrange(0, 3) == 0:
            na = rnd.choice(BIG_NB); nb = rnd.choice(BIG_NB + KEY_NB)
        else:
            na = rnd.choice(KEY_NB); nb = rnd.choice(KEY_NB)
        if rnd.randrange(0, 5) == 0:      # 极端不对称
            nb = rnd.choice([1, 2, 3, 4, 48, 49])
        a = mk_num(na, rnd.randrange(0, 9), rnd)
        b = mk_num(nb, rnd.randrange(0, 9), rnd)
        if rnd.randrange(0, 20) == 0:
            a = 0
        if rnd.randrange(0, 25) == 0:
            b = 0
        sa = -1 if rnd.randrange(0, 3) == 0 else 1
        sb = -1 if rnd.randrange(0, 3) == 0 else 1
        A, B = sa * a, sb * b
        lines.append('%x %x' % (A, B) if A >= 0 and B >= 0
                     else '%s %s' % (fmt(A), fmt(B)))
        exp.append(fmt(A * B))
    return ('%d\n' % T) + '\n'.join(lines) + '\n', '\n'.join(exp) + '\n'


def fmt(v):
    return ('-%x' % -v) if v < 0 else ('%x' % v)


def norm(s):
    return '\n'.join(x.strip().lower() for x in s.strip().split('\n'))


bad = 0
for r in range(ROUNDS):
    rnd = random.Random(SEED0 + r)
    inp, exp = gen_case(rnd, heavy=(r % 4 == 0))
    try:
        p = subprocess.run([BIN], input=inp.encode(), capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        print('ROUND %d: TLE' % r); bad += 1
        open('/tmp/mfail_%d.in' % r, 'w').write(inp)
        if bad >= 3: break
        continue
    got = p.stdout.decode()
    if p.returncode != 0:
        print('ROUND %d: RTE rc=%d stderr=%s' % (r, p.returncode, p.stderr.decode()[:300]))
        open('/tmp/mfail_%d.in' % r, 'w').write(inp)
        bad += 1
        if bad >= 3: break
        continue
    if norm(got) != norm(exp):
        ge, ee = norm(got).split('\n'), norm(exp).split('\n')
        print('ROUND %d: WA (lines got=%d exp=%d)' % (r, len(ge), len(ee)))
        for i in range(min(len(ge), len(ee))):
            if ge[i] != ee[i]:
                il = inp.split('\n')[i + 1].split()
                print('  case#%d A_hex=%d B_hex=%d' % (i, len(il[0]), len(il[1])))
                print('  got: %s ... %s' % (ge[i][:80], ge[i][-40:]))
                print('  exp: %s ... %s' % (ee[i][:80], ee[i][-40:]))
                break
        open('/tmp/mfail_%d.in' % r, 'w').write(inp)
        bad += 1
        if bad >= 3: break
    if (r + 1) % 10 == 0:
        print('  ... %d/%d ok' % (r + 1 - bad, ROUNDS), flush=True)

print('STRESS DONE: rounds=%d bad=%d %s' % (ROUNDS, bad, 'ALL OK' if bad == 0 else 'FAILED'))
sys.exit(1 if bad else 0)
