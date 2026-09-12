# -*- coding: utf-8 -*-
"""VM 端: 多候选 DIV 配对计时 (轮转 + median-of-ratios).

用法: python3 _pair_div_remote.py <bin1> <bin2> [bin3 ...] [--reps N] [--warm N]
- 26 个官方用例全跑
- 每 case: 每 bin 预热 WARM 次(丢弃), 然后 REPS 轮, 每轮按轮转顺序依次跑各 bin
- 输出: 每 bin 的 median ms, 以及相对 bin1 的 median-of-ratios (逐轮配对比值的中位数)
- LC 计分 = 各 case 最大值
"""
import sys, os, subprocess, time, glob, statistics

args = [a for a in sys.argv[1:]]
REPS, WARM = 11, 2
ONLY = None   # --cases c1,c2,... 只跑指定用例 => 省下的时间用来把 reps 拉高。
              # 单 case 的 wall-clock 噪声可达 ±10%, reps=9 全量根本分辨不了 1% 的差异。
out = []
i = 0
while i < len(args):
    if args[i] == '--reps':
        REPS = int(args[i + 1]); i += 2
    elif args[i] == '--warm':
        WARM = int(args[i + 1]); i += 2
    elif args[i] == '--cases':
        ONLY = set(args[i + 1].split(',')); i += 2
    else:
        out.append(args[i]); i += 1
BINS = out
assert len(BINS) >= 2, 'need >=2 bins'

INDIR = os.path.expanduser("~/lcp/big_integer/division_of_big_integers/in")
cases = sorted(glob.glob(os.path.join(INDIR, "*.in")))
if ONLY:
    cases = [c for c in cases if os.path.basename(c)[:-3] in ONLY]
    assert cases, 'no case matched'


def one(binname, cf):
    t0 = time.perf_counter()
    subprocess.run(['bin/' + binname], stdin=open(cf, "rb"),
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    return (time.perf_counter() - t0) * 1000.0


med = {b: {} for b in BINS}
rat = {b: {} for b in BINS}
allrat = {b: {} for b in BINS}   # 逐轮配对比值的完整样本, 用于判显著性

for cf in cases:
    name = os.path.basename(cf)[:-3]
    for b in BINS:
        for _ in range(WARM):
            one(b, cf)
    samples = {b: [] for b in BINS}
    for r in range(REPS):
        order = BINS[r % len(BINS):] + BINS[:r % len(BINS)]   # 轮转
        row = {}
        for b in order:
            row[b] = one(b, cf)
        for b in BINS:
            samples[b].append(row[b])
    base = BINS[0]
    for b in BINS:
        med[b][name] = statistics.median(samples[b])
        rr = [samples[b][k] / samples[base][k] for k in range(REPS)]
        allrat[b][name] = rr
        rat[b][name] = statistics.median(rr)

print('reps=%d warm=%d  bins=%s' % (REPS, WARM, ','.join(BINS)))
hdr = '%-30s' % 'case'
for b in BINS:
    hdr += ' %10s' % b
for b in BINS[1:]:
    hdr += ' %9s' % ('r_' + b)
print(hdr)

order_cases = sorted(med[BINS[0]], key=lambda x: -med[BINS[0]][x])
for name in order_cases:
    line = '%-30s' % name
    for b in BINS:
        line += ' %10.2f' % med[b][name]
    for b in BINS[1:]:
        line += ' %9.3f' % rat[b][name]
    print(line)

print()
for b in BINS:
    mx = max(med[b].values())
    arg = max(med[b], key=lambda x: med[b][x])
    print('SCORE(max) %-8s = %8.2f ms   @%s' % (b, mx, arg))
print()
for b in BINS[1:]:
    rs = sorted(rat[b].values())
    print('median-of-ratios %-8s vs %-8s = %.4f  (min %.3f max %.3f)'
          % (b, BINS[0], statistics.median(rs), rs[0], rs[-1]))

# 逐 case 的比值分布 (判断差异是否显著: IQR 不跨 1.0 才算真差异)
print('\n=== per-case ratio distribution (all %d paired reps) ===' % REPS)
print('%-30s %s' % ('case', ''.join('%28s' % ('r_' + b) for b in BINS[1:])))
for name in order_cases:
    line = '%-30s' % name
    for b in BINS[1:]:
        v = sorted(allrat[b][name])
        q1, q2, q3 = v[len(v) // 4], statistics.median(v), v[len(v) * 3 // 4]
        sig = '*' if (q1 > 1.0 or q3 < 1.0) else ' '
        line += '%s[%.3f %.3f %.3f]      ' % (sig, q1, q2, q3)
    print(line)
print("(* = IQR 不跨 1.0, 差异显著)")
print('DONE')
