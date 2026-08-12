#!/usr/bin/env python3
"""zbench.py — VM 侧统一测量台（Zen3 口径）

三口径:
  wall : 预热 + 交替轮转 (A B C A B C ...) 取中位数     -> 时间趋势, 抗漂移
  perf : instructions:u / cycles:u / cache 事件           -> 宿主 PMU, 仅参考
  cg   : cachegrind Zen3 L1+L2 / L1+L3 双视角             -> I refs 绝对判据 + miss delta

关键约束 (见 ZEN3_CACHE_SIM_ON_INTEL_VM.md):
  * 统一编译 -march=znver3 -mtune=znver3  (LC 上 native==znver3; 本机 native 会开 AVX-512!)
  * cachegrind I refs 完全确定性, 差异 >0.01% 才算真变化
  * cachegrind miss 绝对值不可信, 只看 A/B delta 的方向与量级
  * cycles / wall 不可跨机迁移, 只作本地趋势

用法:
  zbench.py <mode> <datadir> <cases> <src1.cpp|bin1> [src2 ...] [--reps N] [--tag T]
    mode  = wall | perf | cg | all
    cases = 逗号分隔测试点名 (不带 .in)
"""
import sys, os, subprocess, statistics, time, re, shutil

ZFLAGS = ['-O2', '-std=c++23', '-DEVAL', '-DONLINE_JUDGE',
          '-march=znver3', '-mtune=znver3']
BUILD = '/tmp/zb'

# Zen3 EPYC 7B13 缓存几何 (硬编码, 勿从 lscpu 读)
CG_I1 = '32768,8,64'
CG_D1 = '32768,8,64'
CG_L2 = '524288,8,64'       # L1+L2 视角: 多少访问逃出私有 L2
CG_L3 = '33554432,16,64'    # L1+L3 视角: 多少访问真打 DRAM (单 CCX 32MiB)
CG_L3X = '37748736,18,64'   # exclusive 上界 36MiB 包夹


def build(src):
    """src 可以是 .cpp (编译) 或已存在的可执行 (直接用)。返回 (binpath, name)。"""
    name = os.path.basename(src)
    if src.endswith('.cpp'):
        os.makedirs(BUILD, exist_ok=True)
        name = name[:-4]
        out = os.path.join(BUILD, name + '_zn')
        r = subprocess.run(['g++'] + ZFLAGS + ['-o', out, src],
                           capture_output=True)
        if r.returncode != 0:
            print('COMPILE FAIL %s:\n%s' % (src, r.stderr.decode()[:2000]))
            sys.exit(1)
        return out, name
    return src, name


def evex_count(binpath):
    """统计 EVEX (AVX-512) 指令条数 —— 必须为 0，否则与 LC 不同构。"""
    r = subprocess.run('objdump -d %s | grep -cE "^\\s+[0-9a-f]+:\\s+62 "' % binpath,
                       shell=True, capture_output=True)
    try:
        return int(r.stdout.decode().strip())
    except Exception:
        return -1


def verify(binpath, inf, expf):
    with open(inf, 'rb') as f:
        p = subprocess.run([binpath], stdin=f, capture_output=True, timeout=900)
    if p.returncode != 0:
        return 'RC=%d' % p.returncode
    if not os.path.exists(expf):
        return '?'
    return 'OK' if p.stdout.split() == open(expf, 'rb').read().split() else 'DIFF'


# ---------------------------------------------------------------- wall
CPU = os.environ.get('ZB_CPU', '2')          # 绑核, 避免调度迁移


def wall_once(binpath, data):
    t0 = time.perf_counter_ns()
    subprocess.run(['taskset', '-c', CPU, binpath], input=data,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                   timeout=900)
    return (time.perf_counter_ns() - t0) / 1e6


def mode_wall(bins, datadir, cases, reps):
    # 判据用 min: 噪声只会让程序变慢, 不会变快 => min 最接近真实性能
    print('=== WALL (cpu%s, warmup 3 + %d alternating rounds; MIN is the verdict) ==='
          % (CPU, reps))
    for c in cases:
        inf = os.path.join(datadir, c + '.in')
        data = open(inf, 'rb').read()
        acc = {n: [] for _, n in bins}
        for b, n in bins:                       # warmup
            for _ in range(3):
                wall_once(b, data)
        for _ in range(reps):                   # 交替轮转
            for b, n in bins:
                acc[n].append(wall_once(b, data))
        base = None
        print('-- %s' % c)
        for _, n in bins:
            v = sorted(acc[n])
            mn, med = v[0], statistics.median(v)
            p25 = v[len(v) // 4]
            if base is None:
                base = mn
            print('   %-20s MIN=%8.2f ms (rel=%.4f)  p25=%8.2f  med=%8.2f  max=%8.2f'
                  % (n, mn, mn / base, p25, med, v[-1]))


# ---------------------------------------------------------------- perf
PERF_EV = 'instructions:u,cycles:u,L1-dcache-load-misses,LLC-load-misses,page-faults'


def perf_once(binpath, data):
    p = subprocess.run(['perf', 'stat', '-x,', '-e', PERF_EV, '--', binpath],
                       input=data, stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, timeout=900)
    d = {}
    for line in p.stderr.decode('utf-8', 'replace').strip().split('\n'):
        f = line.split(',')
        if len(f) >= 3:
            try:
                d[f[2].strip()] = float(f[0])
            except ValueError:
                pass
    return d


def mode_perf(bins, datadir, cases, reps):
    print('=== PERF (host Tiger Lake PMU — reference only, median of %d) ===' % reps)
    for c in cases:
        data = open(os.path.join(datadir, c + '.in'), 'rb').read()
        print('-- %s' % c)
        for b, n in bins:
            rows = [perf_once(b, data) for _ in range(reps)]
            g = lambda k: statistics.median([r.get(k, 0) for r in rows])
            print('   %-22s Ir=%9.1fM  cyc=%9.1fM  L1dm=%8.1fK  LLCm=%8.1fK  pf=%6.0f'
                  % (n, g('instructions:u') / 1e6, g('cycles:u') / 1e6,
                     g('L1-dcache-load-misses') / 1e3,
                     g('LLC-load-misses') / 1e3, g('page-faults')))


# ---------------------------------------------------------------- cachegrind
CGRE = re.compile(r'==\d+==\s+(I\s+refs|I1\s+misses|D\s+refs|D1\s+misses|'
                  r'LLi misses|LLd misses|LL misses|Mispredicts|Branches):\s+([\d,]+)')


def cg_once(binpath, inf, ll, tag, outdir):
    log = os.path.join(outdir, 'cg.%s.log' % tag)
    cgout = os.path.join(outdir, 'cg.%s.out' % tag)
    with open(inf, 'rb') as fi, open(log, 'wb') as fl:
        subprocess.run(['setarch', os.uname().machine, '-R',
                        'valgrind', '--tool=cachegrind', '--cache-sim=yes',
                        '--branch-sim=yes', '--I1=' + CG_I1, '--D1=' + CG_D1,
                        '--LL=' + ll, '--cachegrind-out-file=' + cgout, binpath],
                       stdin=fi, stdout=subprocess.DEVNULL, stderr=fl, timeout=7200)
    d = {}
    for line in open(log, encoding='utf-8', errors='replace'):
        m = CGRE.search(line)
        if m:
            d[re.sub(r'\s+', ' ', m.group(1))] = int(m.group(2).replace(',', ''))
    return d


def mode_cg(bins, datadir, cases, tag):
    outdir = '/tmp/zbcg.' + tag
    os.makedirs(outdir, exist_ok=True)
    print('=== CACHEGRIND Zen3 (deterministic; I refs = absolute verdict) ===')
    for c in cases:
        inf = os.path.join(datadir, c + '.in')
        print('-- %s' % c)
        base = {}
        for b, n in bins:
            l2 = cg_once(b, inf, CG_L2, '%s.%s.l2' % (n, c), outdir)
            l3 = cg_once(b, inf, CG_L3, '%s.%s.l3' % (n, c), outdir)
            ir = l2.get('I refs', 0)
            d1 = l2.get('D1 misses', 0)
            m2 = l2.get('LLd misses', 0)      # 逃出 512K L2
            m3 = l3.get('LLd misses', 0)      # 真打 DRAM
            dr = l2.get('D refs', 0)
            if not base:
                base = dict(ir=ir, m2=m2, m3=m3)
            print('   %-20s Ir=%9.1fM(%.4f)  Dref=%8.1fM  D1m=%8.1fK  '
                  'L2esc=%8.1fK(%.3f)  DRAM=%8.1fK(%.3f)'
                  % (n, ir / 1e6, ir / max(base['ir'], 1), dr / 1e6, d1 / 1e3,
                     m2 / 1e3, m2 / max(base['m2'], 1),
                     m3 / 1e3, m3 / max(base['m3'], 1)))
    print('cg out: %s' % outdir)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    flags = {a.split('=')[0]: (a.split('=')[1] if '=' in a else '1')
             for a in sys.argv[1:] if a.startswith('--')}
    mode, datadir, cases = args[0], args[1], args[2].split(',')
    srcs = args[3:]
    reps = int(flags.get('--reps', 5))
    tag = flags.get('--tag', str(os.getpid()))

    bins = []
    print('=== BUILD (-march=znver3 -mtune=znver3) ===')
    for s in srcs:
        b, n = build(s)
        ev = evex_count(b)
        st = 'OK' if ev == 0 else '*** %d EVEX (AVX-512) — NOT LC-equivalent!' % ev
        vr = verify(b, os.path.join(datadir, cases[0] + '.in'),
                    os.path.join(datadir, cases[0] + '.exp'))
        print('   %-22s evex=%s  verify[%s]=%s' % (n, st, cases[0], vr))
        bins.append((b, n))
    print()

    if mode in ('wall', 'all'):
        mode_wall(bins, datadir, cases, reps); print()
    if mode in ('perf', 'all'):
        mode_perf(bins, datadir, cases, reps); print()
    if mode in ('cg', 'all'):
        mode_cg(bins, datadir, cases, tag)


if __name__ == '__main__':
    main()
