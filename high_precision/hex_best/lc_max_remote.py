#!/usr/bin/env python3
# LC 全量测试点生成 + 逐点 instructions:u 对比 (HEX div)
# 铁律: LC 取单点 max 时间 -> 必须找到 max 点, 只有 max 点降了才是真优化。
import os, subprocess, sys, glob, shutil

ROOT = os.path.expanduser('~/lcwork')
LC = os.path.join(ROOT, 'lc/big_integer/division_of_hex_big_integers')
COMMON = os.path.join(ROOT, 'lc/common')
CASEDIR = '/tmp/lccases'

# info.toml: name -> number (seed 0..number-1)
TESTS = [
    ('small.cpp', 1), ('medium.cpp', 3), ('large.cpp', 2), ('max.cpp', 3),
    ('a_max_b_random.cpp', 3), ('power.cpp', 1), ('r_nearly_zero.cpp', 3),
    ('length_ratio_integer.cpp', 6), ('burnikel_ziegler_bound.cpp', 4),
]

def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)

def stage():
    if not os.path.isdir(ROOT):
        os.makedirs(ROOT, exist_ok=True)
        r = sh('tar xzf ~/lc_bundle.tgz -C %s' % ROOT)
        if r.returncode: print(r.stderr); sys.exit(1)
    assert os.path.isdir(LC), LC

def build_gens():
    os.makedirs(CASEDIR, exist_ok=True)
    built = []
    for name, _ in TESTS:
        src = os.path.join(LC, 'gen', name)
        if not os.path.isfile(src):
            print('MISSING gen: %s' % name); continue
        exe = os.path.join(CASEDIR, name[:-4])
        r = sh('g++-15 -O2 -std=c++17 -I%s -I%s -o %s %s 2>&1' % (COMMON, LC, exe, src))
        if r.returncode:
            print('BUILD FAIL %s:\n%s' % (name, (r.stdout or '')[:600])); continue
        built.append(name)
    return built

def gen_cases(built):
    cases = []
    for name, num in TESTS:
        if name not in built: continue
        exe = os.path.join(CASEDIR, name[:-4])
        for seed in range(num):
            out = os.path.join(CASEDIR, '%s_%d.in' % (name[:-4], seed))
            if not os.path.isfile(out) or os.path.getsize(out) == 0:
                r = subprocess.run([exe, str(seed)], capture_output=True, text=True)
                if r.returncode != 0 or not r.stdout:
                    print('GEN FAIL %s seed=%d' % (name, seed)); continue
                open(out, 'w').write(r.stdout)
            cases.append(('%s#%d' % (name[:-4], seed), out))
    return cases

def instr(binp, path):
    p = subprocess.run(['perf', 'stat', '-e', 'instructions:u', '-x,', binp],
                       stdin=open(path), capture_output=True, text=True)
    for line in p.stderr.splitlines():
        if 'instructions:u' in line:
            try: return int(line.split(',')[0])
            except ValueError: pass
    return None

def main():
    bins = sys.argv[1:] or ['/tmp/dv_0_32', '/tmp/dv_1_32']
    stage()
    built = build_gens()
    cases = gen_cases(built)
    print('cases: %d\n' % len(cases))
    hdr = '%-28s' % 'case'
    for b in bins: hdr += ' %15s' % os.path.basename(b)
    if len(bins) == 2: hdr += ' %9s' % 'delta%'
    print(hdr)
    tot = [0] * len(bins)
    mx = [(0, '')] * len(bins)
    for tag, path in cases:
        row = '%-28s' % tag
        vals = []
        for i, b in enumerate(bins):
            v = instr(b, path) or 0
            vals.append(v); tot[i] += v
            if v > mx[i][0]: mx[i] = (v, tag)
            row += ' %15d' % v
        if len(vals) == 2 and vals[0]:
            row += ' %+8.3f%%' % ((vals[1] - vals[0]) / vals[0] * 100.0)
        print(row, flush=True)
    print('')
    for i, b in enumerate(bins):
        print('%-15s MAX = %d  @ %s   (sum=%d)' % (os.path.basename(b), mx[i][0], mx[i][1], tot[i]))
    if len(bins) == 2 and mx[0][0]:
        print('MAX-point delta: %+.3f%%' % ((mx[1][0] - mx[0][0]) / mx[0][0] * 100.0))

main()
