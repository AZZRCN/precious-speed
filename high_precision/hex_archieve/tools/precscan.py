#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
precscan.py — FFT 位宽 k 的精度边界扫描（必须在 VM 上跑）

对每个规模 u（结果 limb 数），从大到小尝试 k，用最坏输入（全 f）测
max |g[j] - round(g[j])|，找出满足 max_err < LIMIT 的最大 k。

然后算出该 k 下的 lm，跟现役 gk 表给出的 lm 对比，直接量化可省多少。

用法: python3 precscan.py /tmp/v10c_prec [limit]
"""
import os
import sys
import subprocess

BIN = sys.argv[1]
LIMIT = float(sys.argv[2]) if len(sys.argv) > 2 else 0.25
WORK = '/dev/shm/prec'
os.makedirs(WORK, exist_ok=True)

GK = [19 << 4, 18 << 6, 17 << 8, 16 << 10, 15 << 12,
      14 << 14, 13 << 16, 12 << 18, 11 << 20, 10 << 22, 1 << 62]


def cur_k(u):
    i = 0
    while u > GK[i]:
        i += 1
    return 19 - i


def lm_of(u, k):
    c = u * 64 // k + 1
    return 2 << (c.bit_length() - 1), c


def make_worst(la_hex, path, T=1):
    """最坏输入：两个操作数全 f"""
    s = 'f' * la_hex
    open(path, 'w').write('%d\n' % T + ('%s %s\n' % (s, s)) * T)


def probe(path, k):
    env = dict(os.environ)
    env['FORCE_K'] = str(k)
    with open(path) as fin, open('/dev/null', 'w') as fout:
        r = subprocess.run([BIN], stdin=fin, stdout=fout,
                           stderr=subprocess.PIPE, text=True, env=env)
    for ln in r.stderr.splitlines():
        if ln.startswith('PROBE'):
            d = dict(x.split('=') for x in ln.split()[1:])
            return float(d['maxerr']), int(d['lm']), int(d['coeffs'])
    return -1.0, 0, 0


# la_hex 列表 -> u = 2 * ceil(la/16)
SIZES = [1024, 4096, 16384, 32768, 49152, 65536, 98304, 131072,
         196608, 262144, 393216, 524288, 786432, 1048576, 1300000, 1600000]

print('LIMIT max_err < %.3f' % LIMIT)
print('%9s %8s %4s %9s | %4s %9s %9s | %s' %
      ('hex', 'u', 'k_now', 'lm_now', 'k_max', 'lm_best', 'maxerr', 'GAIN'))
tot_now = tot_best = 0
for h in SIZES:
    u = 2 * ((h + 15) // 16)
    kn = cur_k(u)
    lmn, cn = lm_of(u, kn)
    p = '%s/w_%d.in' % (WORK, h)
    make_worst(h, p)
    best_k, best_lm, best_err = kn, lmn, None
    # 从 19 往下找第一个满足精度的 k（k 越大 lm 越小）
    for k in range(19, kn - 1, -1):
        err, lm, c = probe(p, k)
        if err < 0 or err >= LIMIT:
            continue
        best_k, best_lm, best_err = k, lm, err
        break
    if best_err is None:
        err, lm, c = probe(p, kn)
        best_err = err
        best_lm = lm
    gain = '' if best_lm >= lmn else '  <-- 省 %.0f%%' % (100 * (1 - best_lm / lmn))
    print('%9d %8d %4d %9d | %4d %9d %9.5f | %s' %
          (h, u, kn, lmn, best_k, best_lm, best_err, gain))
    sys.stdout.flush()
    tot_now += lmn
    tot_best += best_lm
    os.remove(p)
print('---')
print('sum(lm): now=%d best=%d  ->  %.1f%% less FFT work' %
      (tot_now, tot_best, 100 * (1 - tot_best / tot_now)))
