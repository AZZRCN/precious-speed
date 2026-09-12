#!/usr/bin/env python3
# 在 VM 上运行: 编译 fftbench 并用 callgrind 差分测各 FFT 原语的 Ir / est
import subprocess, os, sys, math

REMOTE = '/home/azzr/divbench'
SRCS = sys.argv[1].split(',') if len(sys.argv) > 1 else ['div_D16']
ONLY_OPS = sys.argv[2].split(',') if len(sys.argv) > 2 else ['dif', 'idit', 'dot', 'conv']
CACHE = '--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64'
R_HI, R_LO = 9, 1


def build(src, tag, extra=''):
    out = REMOTE + '/bin/fb_' + tag
    define = '-DSRC_MAIN=' + "'" + '"src/' + src + '.cpp"' + "'"
    cmd = 'g++ -O2 -std=c++23 -march=x86-64-v3 %s %s -o %s %s/fftbench.cpp' % (
        define, extra, out, REMOTE)
    r = subprocess.run(cmd, shell=True, cwd=REMOTE, capture_output=True, text=True)
    if r.returncode != 0:
        print('BUILD FAIL', tag)
        print(r.stderr[-3000:])
        return None
    return out


def measure(binp, fl, reps, op, tag):
    outf = '/tmp/fb_%s_%d_%d_%s.out' % (tag, fl, reps, op)
    cmd = ('valgrind --tool=callgrind ' + CACHE + ' --callgrind-out-file=' + outf +
           ' ' + binp + ' %d %d %s >/dev/null 2>/dev/null' % (fl, reps, op))
    if subprocess.run(cmd, shell=True, cwd=REMOTE).returncode != 0:
        return None
    ev = tot = None
    for line in open(outf):
        if line.startswith('events:'):
            ev = line.split()[1:]
        elif line.startswith('summary:') or line.startswith('totals:'):
            tot = [int(x) for x in line.split()[1:]]
    os.remove(outf)
    if not ev or not tot:
        return None
    return dict(zip(ev, tot))


def W2(fl):
    N = fl // 2
    return N * math.log2(N)


def W3(fl):
    m = fl // 6
    blk = m * 2
    N = blk // 2
    return 3 * N * math.log2(N) + 3 * m * math.log2(3)


JOBS = [(98304, 'fft3'), (131072, '2pow'),
        (49152, 'fft3'), (65536, '2pow'),
        (196608, 'fft3'), (262144, '2pow')]

for src in SRCS:
    b = build(src, src)
    if not b:
        continue
    print('#### %s' % src, flush=True)
    print('%-8s %-6s %-5s %14s %12s %9s %9s %11s %9s' % (
        'fl', 'kind', 'op', 'Ir/call', 'W', 'Ir/W', 'est/W', 'D1m', 'DLm'), flush=True)
    data = {}
    for fl, kind in JOBS:
        W = W3(fl) if kind == 'fft3' else W2(fl)
        for op in ONLY_OPS:
            hi = measure(b, fl, R_HI, op, src)
            lo = measure(b, fl, R_LO, op, src)
            if not hi or not lo:
                print('%-8d %-6s %-5s FAIL' % (fl, kind, op), flush=True)
                continue
            n = R_HI - R_LO
            ir = (hi['Ir'] - lo['Ir']) / n
            d1 = ((hi.get('D1mr', 0) + hi.get('D1mw', 0)) -
                  (lo.get('D1mr', 0) + lo.get('D1mw', 0))) / n
            dl = ((hi.get('DLmr', 0) + hi.get('DLmw', 0)) -
                  (lo.get('DLmr', 0) + lo.get('DLmw', 0))) / n
            est = ir + 5 * d1 + 200 * dl
            data[(fl, op)] = (ir, W, est)
            print('%-8d %-6s %-5s %14.0f %12.0f %9.4f %9.4f %11.0f %9.0f' % (
                fl, kind, op, ir, W, ir / W, est / W, d1, dl), flush=True)
    print(flush=True)
    print('=== 每单位 W 效率比 (fft3 / 2pow, >1 表示 radix-3 路径更低效) ===', flush=True)
    for a, c in [(98304, 131072), (49152, 65536), (196608, 262144)]:
        for op in ONLY_OPS:
            if (a, op) in data and (c, op) in data:
                ia, Wa, ea = data[(a, op)]
                ic, Wc, ec = data[(c, op)]
                print('  %-5s %6d vs %6d :  Ir/W %.4f vs %.4f -> %+.2f%%   est/W %+.2f%%' % (
                    op, a, c, ia / Wa, ic / Wc, 100 * ((ia / Wa) / (ic / Wc) - 1),
                    100 * ((ea / Wa) / (ec / Wc) - 1)), flush=True)
    print(flush=True)
    print('=== 绝对代价对比 (同一 conv 任务: fft3 档 vs 2幂档) ===', flush=True)
    for a, c in [(98304, 131072), (49152, 65536), (196608, 262144)]:
        if (a, 'conv') in data and (c, 'conv') in data:
            ia, Wa, ea = data[(a, 'conv')]
            ic, Wc, ec = data[(c, 'conv')]
            print('  conv %6d Ir=%.0f est=%.0f   |  %6d Ir=%.0f est=%.0f  |  '
                  'fft3 省 Ir %.2f%% / est %.2f%% (理论 W 省 %.2f%%)' % (
                      a, ia, ea, c, ic, ec, 100 * (1 - ia / ic), 100 * (1 - ea / ec),
                      100 * (1 - Wa / Wc)), flush=True)
    print(flush=True)
print('DONE', flush=True)
