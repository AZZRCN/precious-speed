#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mul 分档梯度扫描 (必须在 VM 上跑, 本地 Windows 数据不可信)

用法:
    python3 gridscan_mul.py <bin> [mode]
      mode = bal   平衡扫描 la==lb
             unbal 不平衡扫描 la 固定, lb 递减
             all   两个都跑 (默认)

输出: 每个 (la,lb) 的 ns/case, 用于定位 comba<->FFT 切换阈值与
      不平衡输入下 FFT 补零浪费的悬崖。
"""
import os, sys, subprocess, binascii, time

WORK = '/dev/shm/grid'
os.makedirs(WORK, exist_ok=True)


def rnd_hex(n):
    s = binascii.hexlify(os.urandom((n + 1) // 2)).decode()[:n]
    if s[0] == '0':
        s = '1' + s[1:]
    return s


def gen(la, lb, T, path):
    buf = [str(T)]
    for _ in range(T):
        buf.append(rnd_hex(la) + ' ' + rnd_hex(lb))
    with open(path, 'w') as f:
        f.write('\n'.join(buf) + '\n')


def timeit(binp, path, reps=3):
    """返回 task-clock 毫秒 (perf -r reps 的均值)"""
    cmd = ['perf', 'stat', '-x,', '-e', 'task-clock', '-r', str(reps), binp]
    with open(path) as fin, open('/dev/null', 'w') as fout:
        r = subprocess.run(cmd, stdin=fin, stdout=fout,
                           stderr=subprocess.PIPE, text=True)
    for line in r.stderr.splitlines():
        if 'task-clock' in line:
            return float(line.split(',')[0])
    return -1.0


def run_set(binp, cases, tag):
    print('=== %s ===' % tag)
    print('%9s %9s %6s %10s %12s %10s' %
          ('la', 'lb', 'T', 'ms_total', 'us_per_case', 'hexPerUs'))
    for la, lb, T in cases:
        p = '%s/g_%d_%d.in' % (WORK, la, lb)
        gen(la, lb, T, p)
        ms = timeit(binp, p)
        us = ms * 1000.0 / T
        thr = (la + lb) / us if us > 0 else 0
        print('%9d %9d %6d %10.2f %12.2f %10.1f' % (la, lb, T, ms, us, thr))
        sys.stdout.flush()
        os.remove(p)


def main():
    binp = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else 'all'
    TOTAL = 3_000_000          # 每档总 hex 量, 对齐 large 系列量级

    if mode in ('bal', 'all'):
        # 平衡扫描: 密集覆盖 comba(mb<=48 limb=768hex) <-> FFT 交界
        lens = [128, 256, 384, 512, 640, 768, 896, 1024, 1280, 1536, 2048,
                3072, 4096, 6144, 8192, 12288, 16384, 32768, 65536,
                262144, 1048576, 1600000]
        cases = []
        for L in lens:
            T = max(1, min(4000, TOTAL // (2 * L)))
            cases.append((L, L, T))
        run_set(binp, cases, 'BALANCED  la == lb')

    if mode in ('unbal', 'all'):
        # 不平衡扫描: la 固定大, lb 递减 -> 暴露 FFT 补零浪费
        for LA in (65536, 1600000):
            cases = []
            lb = LA
            while lb >= 64:
                T = max(1, min(2000, TOTAL // (LA + lb)))
                cases.append((LA, lb, T))
                lb //= 4
            run_set(binp, cases, 'UNBALANCED la=%d' % LA)


if __name__ == '__main__':
    main()
