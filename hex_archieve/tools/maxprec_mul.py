#!/usr/bin/env python3
"""mul 满规模精度压测: 用 LC 上限尺寸 (1.6M hex chars) 的最恶劣模式验证 FFT 舍入.

用法: python3 maxprec_mul.py <bin> [hexlen] [rounds]
模式: 全 F / 交替 / 随机高位 / 稀疏尖峰 / 全 F 平方
"""
import random, subprocess, sys

BIN = sys.argv[1]
HL = int(sys.argv[2]) if len(sys.argv) > 2 else 1600000   # hex chars per operand
R = int(sys.argv[3]) if len(sys.argv) > 3 else 6

rnd = random.Random(31337)
NB = HL * 4


def pat(mode):
    if mode == 0:
        return (1 << NB) - 1                              # 全 F: 系数最大
    if mode == 1:
        return int('f0' * (NB // 8), 16)                  # 交替
    if mode == 2:
        return rnd.getrandbits(NB) | (1 << (NB - 1))      # 随机
    if mode == 3:
        v = (1 << (NB - 1))
        for _ in range(64):
            v |= 1 << rnd.randrange(0, NB - 1)
        return v                                          # 稀疏
    if mode == 4:
        return int('ff00' * (NB // 16), 16) | (1 << (NB - 1))
    return int('e' * HL, 16)


bad = 0
for m in range(R):
    a = pat(m % 6)
    b = pat((m + 1) % 6) if m == 0 else pat((m + 3) % 6)   # 第 0 轮 = 全 F x 交替
    if m == 1:
        b = a                                              # 平方式 (a==b 数值相同)
    e = '%x' % (a * b)
    inp = '1\n%x %x\n' % (a, b)
    p = subprocess.run([BIN], input=inp.encode(), capture_output=True, timeout=600)
    g = p.stdout.decode().strip()
    if g.lower() != e:
        bad += 1
        d = next((j for j in range(min(len(g), len(e))) if g[j] != e[j]), -1)
        print('CASE %d MISMATCH  in=%dB len got=%d exp=%d  first diff @%d'
              % (m, len(inp), len(g), len(e), d))
        print('  got ...%s...' % g[max(0, d - 20):d + 20])
        print('  exp ...%s...' % e[max(0, d - 20):d + 20])
    else:
        print('CASE %d OK  (in=%dB, out=%d hex digits)' % (m, len(inp), len(e)), flush=True)
print('MAXPREC: %s' % ('ALL OK' if bad == 0 else 'FAILED bad=%d' % bad))
sys.exit(1 if bad else 0)
