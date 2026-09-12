#!/usr/bin/env python3
"""HEX 加法暴力对拍: 随机 + 边界构造, 与 python int 数学等价对比.

用法: python3 stress_add.py <bin> [rounds] [seed]
覆盖: SIMD 16-hex 块解析边界 / 尾巴 L=1..15 / 进位链全传播 (0xFFF..+1)
      借位链全传播 / 同号加 / 异号减 / 等值相消 -> 0 / 符号 / 前导零 / 零值
      长度极端不对称 / 单 limb / 跨 limb 进位
"""
import random, subprocess, sys

BIN = sys.argv[1]
ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 100
SEED0 = int(sys.argv[3]) if len(sys.argv) > 3 else 20260810

# 以 hex 位数为单位取关键规模 (16 hex = 1 limb, SIMD 一次吃 16 hex)
KEY_H = [1, 2, 3, 7, 8, 9, 15, 16, 17, 18, 31, 32, 33, 47, 48, 49,
         63, 64, 65, 95, 96, 97, 127, 128, 129, 255, 256, 257,
         511, 512, 513, 1023, 1024, 1025]
BIG_H = [4095, 4096, 4097, 8191, 8192, 8193, 16384, 16385, 32768, 65536]


def mk_num(nh, mode, rnd):
    """生成恰好 nh 个 hex 位 (最高位非零) 的正整数, 或特殊形态."""
    nb = 4 * nh
    if mode == 0:
        v = rnd.getrandbits(nb)
    elif mode == 1:                       # 顶位置 1, 满长度
        v = rnd.getrandbits(nb) | (1 << (nb - 1))
    elif mode == 2:                       # 全 F -> 加法必然全链进位
        v = (1 << nb) - 1
    elif mode == 3:                       # 1000...0
        v = 1 << (nb - 1)
    elif mode == 4:                       # 0xFFFF...FFFE
        v = ((1 << nb) - 1) ^ 1
    elif mode == 5:                       # 单 bit
        v = 1 << rnd.randrange(0, nb)
    elif mode == 6:                       # 高半 F 低半 0
        k = nb // 2
        v = ((1 << (nb - k)) - 1) << k
        v |= 1 << (nb - 1)
    elif mode == 7:                       # 交替 0xF0F0
        v = int('f0' * max(nh // 2, 1), 16)
    elif mode == 8:                       # limb 边界处一堆 F (跨 limb 进位)
        v = rnd.getrandbits(nb)
        for lb in range(0, nb, 64):
            if rnd.randrange(0, 2):
                v |= ((1 << min(64, nb - lb)) - 1) << lb
    else:                                 # 很小的值 (制造前导零 limb)
        v = rnd.getrandbits(min(nb, 8))
    return max(v, 1)


def fmt(v):
    return ('-%x' % -v) if v < 0 else ('%x' % v)


def gen_case(rnd, heavy):
    lines, exp = [], []
    T = rnd.randrange(1, 12)
    for _ in range(T):
        if heavy and rnd.randrange(0, 3) == 0:
            na = rnd.choice(BIG_H); nb = rnd.choice(BIG_H + KEY_H)
        else:
            na = rnd.choice(KEY_H); nb = rnd.choice(KEY_H)
        if rnd.randrange(0, 5) == 0:      # 极端不对称
            nb = rnd.choice([1, 2, 3, 16, 17])
        a = mk_num(na, rnd.randrange(0, 10), rnd)
        b = mk_num(nb, rnd.randrange(0, 10), rnd)
        if rnd.randrange(0, 18) == 0:
            a = 0
        if rnd.randrange(0, 22) == 0:
            b = 0
        sa = -1 if rnd.randrange(0, 3) == 0 else 1
        sb = -1 if rnd.randrange(0, 3) == 0 else 1
        A, B = sa * a, sb * b
        if rnd.randrange(0, 12) == 0:     # 等值相消 -> 结果 0
            B = -A
        if rnd.randrange(0, 15) == 0:     # 只差 1 的相消
            B = -A + (1 if rnd.randrange(0, 2) else -1)
        lines.append('%s %s' % (fmt(A), fmt(B)))
        exp.append(fmt(A + B))
    return ('%d\n' % T) + '\n'.join(lines) + '\n', '\n'.join(exp) + '\n'


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
        open('/tmp/afail_%d.in' % r, 'w').write(inp)
        if bad >= 3: break
        continue
    got = p.stdout.decode()
    if p.returncode != 0:
        print('ROUND %d: RTE rc=%d stderr=%s' % (r, p.returncode, p.stderr.decode()[:300]))
        open('/tmp/afail_%d.in' % r, 'w').write(inp)
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
        open('/tmp/afail_%d.in' % r, 'w').write(inp)
        bad += 1
        if bad >= 3: break
    if (r + 1) % 20 == 0:
        print('  ... %d/%d ok' % (r + 1 - bad, ROUNDS), flush=True)

print('STRESS DONE: rounds=%d bad=%d %s' % (ROUNDS, bad, 'ALL OK' if bad == 0 else 'FAILED'))
sys.exit(1 if bad else 0)
