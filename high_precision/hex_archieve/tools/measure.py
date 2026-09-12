#!/usr/bin/env python3
"""VM 侧: 对若干候选二进制在官方测试点上做 正确性验证 + perf 指令数/周期 测量.

用法:
  measure.py <prob> <datadir> <bin1> [bin2 ...] [--reps N] [--verify-only] [--no-verify]

判据: LC 计分取 **最长测试点** 的 CPU 时间. 主指标 = 该测试点的 instructions(Ir),
      辅以 cycles 校验 (cache miss 影响不体现在 Ir 上).
"""
import sys, os, glob, subprocess, re, statistics

def run_perf(binpath, inp, reps):
    irs, cys, secs = [], [], []
    for _ in range(reps):
        with open(inp, 'rb') as fi:
            p = subprocess.run(
                ['perf', 'stat', '-x,', '-e', 'instructions:u,cycles:u,task-clock',
                 '--', binpath],
                stdin=fi, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=900)
        err = p.stderr.decode('utf-8', 'replace')
        d = {}
        for line in err.strip().split('\n'):
            f = line.split(',')
            if len(f) >= 3:
                try:
                    d[f[2].strip()] = float(f[0])
                except ValueError:
                    pass
        irs.append(d.get('instructions:u', 0))
        cys.append(d.get('cycles:u', 0))
        secs.append(d.get('task-clock', 0))
    return statistics.median(irs), statistics.median(cys), statistics.median(secs)

def verify(binpath, inp, exp):
    with open(inp, 'rb') as fi:
        p = subprocess.run([binpath], stdin=fi, capture_output=True, timeout=900)
    if p.returncode != 0:
        return f'RC={p.returncode}'
    got = p.stdout.upper().split()
    want = open(exp, 'rb').read().upper().split()
    if got == want:
        return 'OK'
    n = min(len(got), len(want))
    for i in range(n):
        if got[i] != want[i]:
            return f'DIFF@tok{i} exp={want[i][:40]!r} got={got[i][:40]!r}'
    return f'LEN got={len(got)} want={len(want)}'

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    flags = [x for x in sys.argv[1:] if x.startswith('--')]
    reps = 3
    for f in flags:
        if f.startswith('--reps'):
            reps = int(f.split('=')[1]) if '=' in f else 3
    prob, datadir = a[0], a[1]
    bins = a[2:]
    do_verify = '--no-verify' not in flags
    only_verify = '--verify-only' in flags

    cases = sorted(glob.glob(os.path.join(datadir, '*.in')))
    names = [os.path.basename(c)[:-3] for c in cases]
    labels = [os.path.basename(b) for b in bins]

    if do_verify:
        print('=== VERIFY ===')
        bad = 0
        for b, lb in zip(bins, labels):
            probs = []
            for c, nm in zip(cases, names):
                exp = c[:-3] + '.exp'
                if not os.path.exists(exp):
                    probs.append(f'{nm}:NOEXP'); continue
                r = verify(b, c, exp)
                if r != 'OK':
                    probs.append(f'{nm}:{r}')
            if probs:
                bad += 1
                print(f'  {lb}: FAIL')
                for x in probs[:6]:
                    print(f'      {x}')
            else:
                print(f'  {lb}: ALL OK ({len(cases)} cases)')
        if only_verify:
            return 1 if bad else 0

    print('=== PERF (median of %d) ===' % reps)
    res = {}
    for b, lb in zip(bins, labels):
        res[lb] = {}
        for c, nm in zip(cases, names):
            res[lb][nm] = run_perf(b, c, reps)
            print(f'  . {lb}/{nm}', flush=True)

    w = max(len(n) for n in names) + 2
    hdr = 'case'.ljust(w) + ''.join(f'{lb+" Ir":>14}{lb+" cyc":>14}' for lb in labels)
    print()
    print(hdr)
    print('-' * len(hdr))
    for nm in names:
        row = nm.ljust(w)
        for lb in labels:
            ir, cy, _ = res[lb][nm]
            row += f'{ir/1e6:>13.1f}M{cy/1e6:>13.1f}M'
        print(row)
    print()
    print('--- MAX test point (LC scoring metric) ---')
    for lb in labels:
        mx = max(res[lb].items(), key=lambda kv: kv[1][0])
        mxc = max(res[lb].items(), key=lambda kv: kv[1][1])
        print(f'  {lb:>16}: maxIr={mx[1][0]/1e6:9.1f}M @{mx[0]}   '
              f'maxCyc={mxc[1][1]/1e6:9.1f}M @{mxc[0]}  ({mxc[1][2]:.1f}ms)')
    if len(labels) >= 2:
        b0 = max(res[labels[0]].values(), key=lambda v: v[0])[0]
        print()
        for lb in labels[1:]:
            b1 = max(res[lb].values(), key=lambda v: v[0])[0]
            print(f'  {lb} vs {labels[0]}: Ir ratio {b1/b0:.4f} '
                  f'({"FASTER" if b1 < b0 else "slower"} {abs(1-b1/b0)*100:.1f}%)')
    return 0

if __name__ == '__main__':
    sys.exit(main())
