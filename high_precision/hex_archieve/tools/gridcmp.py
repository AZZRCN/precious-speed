#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gridcmp.py — 两个二进制在一组 (la, lb) 组合上的 A/B 对比（必须在 VM 上跑）

用法:
    python3 gridcmp.py <binA> <binB> [set]
      set = unbal (默认) | bal | large | small

输出每个组合的 A ms / B ms / 比值 (B/A, <1 表示 B 更快)。
每个组合都是 ABBA 交替 + 取最小值，抗热漂移。
"""
import os
import sys
import subprocess
import binascii

WORK = '/dev/shm/gcmp'
os.makedirs(WORK, exist_ok=True)


def rnd_hex(n):
    s = binascii.hexlify(os.urandom((n + 1) // 2)).decode()[:n]
    return ('1' + s[1:]) if s[0] == '0' else s


def gen(la, lb, T, path):
    buf = [str(T)]
    for _ in range(T):
        buf.append(rnd_hex(la) + ' ' + rnd_hex(lb))
    open(path, 'w').write('\n'.join(buf) + '\n')


def one(binp, path):
    cmd = ['perf', 'stat', '-x,', '-e', 'task-clock', '-r', '3', binp]
    with open(path) as fin, open('/dev/null', 'w') as fout:
        r = subprocess.run(cmd, stdin=fin, stdout=fout,
                           stderr=subprocess.PIPE, text=True)
    for line in r.stderr.splitlines():
        if 'task-clock' in line:
            return float(line.split(',')[0])
    return -1.0


def check_same(ba, bb, path):
    """顺带核对两个二进制输出是否逐字节一致"""
    with open(path) as fin:
        ra = subprocess.run([ba], stdin=fin, stdout=subprocess.PIPE).stdout
    with open(path) as fin:
        rb = subprocess.run([bb], stdin=fin, stdout=subprocess.PIPE).stdout
    return ra == rb


SETS = {
    # (la, lb, T)  —— hex 位数
    'unbal': [
        (4096, 2048, 400), (4096, 1024, 400), (4096, 256, 400),
        (16384, 8192, 120), (16384, 4096, 150), (16384, 1024, 200), (16384, 256, 300),
        (65536, 32768, 30), (65536, 16384, 40), (65536, 4096, 50), (65536, 1024, 60),
        (262144, 131072, 8), (262144, 65536, 10), (262144, 16384, 14), (262144, 4096, 20),
        (1600000, 800000, 2), (1600000, 400000, 2), (1600000, 100000, 3),
        (1600000, 25000, 4), (1600000, 6250, 5),
    ],
    'bal': [
        (1024, 1024, 800), (4096, 4096, 300), (16384, 16384, 90),
        (65536, 65536, 22), (262144, 262144, 5), (1048576, 1048576, 1),
        (1600000, 1600000, 1),
    ],
    # 模拟 LC large_* 的混合分布
    'large': [
        (100000, 1000, 30), (100000, 10000, 30), (50000, 2000, 40),
        (30000, 30000, 60), (80000, 5000, 30),
    ],
    # 命中 '长度浪费' 修正点的规模 (u = 2*ceil(la/16) 落在 76/152/304/576/1152/2176/4352/8192/16384/30720/61440/114688)
    'gap': [
        (34816, 34816, 40),    # u=4352
        (65536, 65536, 22),    # u=8192   <- VM 扫描里的深谷
        (131072, 131072, 10),  # u=16384
        (245760, 245760, 6),   # u=30720
        (491520, 491520, 3),   # u=61440
        (917504, 917504, 2),   # u=114688
        # 邻近非命中点做对照, 应无变化
        (60000, 60000, 24),
        (120000, 120000, 11),
    ],
    'small': [
        (64, 64, 4000), (128, 128, 4000), (256, 256, 3000),
        (512, 512, 2000), (1024, 512, 1500), (2048, 256, 1000),
    ],
}

if __name__ == '__main__':
    ba, bb = sys.argv[1], sys.argv[2]
    key = sys.argv[3] if len(sys.argv) > 3 else 'unbal'
    cases = SETS[key]
    print('A = %s' % ba)
    print('B = %s' % bb)
    print('=== SET %s ===' % key)
    print('%9s %9s %6s %9s %9s %8s %6s' %
          ('la', 'lb', 'T', 'A_ms', 'B_ms', 'B/A', 'same'))
    for la, lb, T in cases:
        p = '%s/c_%d_%d.in' % (WORK, la, lb)
        gen(la, lb, T, p)
        same = check_same(ba, bb, p)
        a1 = one(ba, p)
        b1 = one(bb, p)
        b2 = one(bb, p)
        a2 = one(ba, p)
        a = min(a1, a2)
        b = min(b1, b2)
        print('%9d %9d %6d %9.2f %9.2f %8.4f %6s' %
              (la, lb, T, a, b, (b / a if a > 0 else 0), 'OK' if same else '**DIFF**'))
        sys.stdout.flush()
        os.remove(p)
