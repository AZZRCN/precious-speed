#!/usr/bin/env python3
# -DCEILHIST 编译并跑用例: 取 fft_ceil 的 need->used 浪费全量 CSV, 并离线模拟
# 「若增加 5*2^k / 7*2^k 档位」能再省多少 N*logN
import subprocess, os, sys, math, csv

REMOTE = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRCS = (sys.argv[1] if len(sys.argv) > 1 else 'div_D16').split(',')
CASES = (sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03').split(',')
os.chdir(REMOTE)
os.makedirs('bin', exist_ok=True)


def ladder_ceil(n, mults):
    """给定允许的奇数因子集合 mults, 返回 >= n 的最小 m*2^k"""
    best = None
    for m in mults:
        v = m
        while v < n:
            v <<= 1
        if best is None or v < best:
            best = v
    return best


LADDERS = [
    ('2pow only      {1}', {1}),
    ('current        {1,3}', {1, 3}),
    ('+radix5        {1,3,5}', {1, 3, 5}),
    ('+radix5,7      {1,3,5,7}', {1, 3, 5, 7}),
    ('+r5,7,9        {1,3,5,7,9}', {1, 3, 5, 7, 9}),
    ('+r5,7,9,11,13  {1,3,5,7,9,11,13}', {1, 3, 5, 7, 9, 11, 13}),
]

for src in SRCS:
    tag = 'ch_' + src
    cmd = ('g++ -O2 -std=c++23 -march=x86-64-v3 -DCEILHIST -o bin/%s src/%s.cpp' % (tag, src))
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if r.returncode != 0:
        print('COMPILE FAIL', src); print(r.stderr[-3000:]); continue
    for case in CASES:
        csvf = '/tmp/ceil_%s_%s.csv' % (src, case)
        env = dict(os.environ); env['CEILHIST_CSV'] = csvf
        p = subprocess.run('./bin/%s < %s%s.in > /dev/null' % (tag, IN, case),
                           shell=True, capture_output=True, text=True, env=env)
        print('##### %s / %s' % (src, case), flush=True)
        print(p.stderr, flush=True)
        if not os.path.exists(csvf):
            print('  (no csv)'); continue
        rows = [r for r in csv.DictReader(open(csvf)) if int(r['need']) >= 2]

        res = []
        for name, mults in LADDERS:
            tot = sum(ladder_ceil(int(r['need']), mults) *
                      math.log2(ladder_ceil(int(r['need']), mults)) * int(r['cnt'])
                      for r in rows)
            res.append((name, tot))
        cur = dict(res)['current        {1,3}']
        ideal = sum(int(r['need']) * math.log2(int(r['need'])) * int(r['cnt']) for r in rows)

        print('  --- 档位阶梯离线模拟 (相对当前 {1,3}) ---')
        for name, tot in res:
            print('    %-34s sum(NlogN)=%.6g   %+.2f%%' % (name, tot, 100.0 * (tot / cur - 1.0)))
        print('    %-34s sum(NlogN)=%.6g   %+.2f%%  <= 理论下界(无量化)'
              % ('ideal', ideal, 100.0 * (ideal / cur - 1.0)))

        # 按 tag 分类看浪费来源
        print('  --- 按语义类别拆分当前浪费 ---')
        TAG = {'0': 'lin', '1': 'mn', '2': 'cycm'}
        agg = {}
        for r in rows:
            t = TAG.get(r['tag'], r['tag'])
            need, used, cnt = int(r['need']), int(r['used']), int(r['cnt'])
            a = agg.setdefault(t, [0.0, 0.0])
            a[0] += used * math.log2(used) * cnt
            a[1] += need * math.log2(need) * cnt
        for t, (u, n) in sorted(agg.items()):
            print('    %-6s used=%.5g ideal=%.5g  overhead=%+.2f%%  (占总used %.1f%%)'
                  % (t, u, n, 100.0 * (u / n - 1.0), 100.0 * u / cur))
        print('----- end -----\n', flush=True)
