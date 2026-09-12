#!/usr/bin/env python3
"""D14 unwrap 诊断: 量化 cx/cy 分布, 判定 ±1 修正为何破坏 medium_02。

4 个变体 (全部带 -DUNWRAP_PROBE):
  g2m0 = -DGATE_POW2 -DUNWRAP_MODE=0   (== D13 行为, 只是加了探针)
  g2m2 = -DGATE_POW2 -DUNWRAP_MODE=2   (== d14p, 已知 medium_02 挂)
  g3m0 = -DUNWRAP_MODE=0               (放宽闸门, 不修正)
  g3m2 = -DUNWRAP_MODE=2               (放宽闸门 + 修正)

对 26 例: md5 对拍 d13, 并统计 stderr 中 [unwrap] 行的 (cx,cy) 组合。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"

put(os.path.join(PS, "best", "div_D13.cpp"), "/home/azzr/divbench/src/d13.cpp")
put(os.path.join(PS, "best", "div_D14.cpp"), "/home/azzr/divbench/src/d14.cpp")

G = "g++ -O2 -std=c++23 -march=x86-64-v3"
build = (
    "cd ~/divbench/src && "
    f"{G} -o ../bin/d13 d13.cpp || echo BUILDFAIL_d13; "
    f"{G} -DUNWRAP_PROBE -DGATE_POW2 -DUNWRAP_MODE=0 -o ../bin/g2m0 d14.cpp || echo BUILDFAIL_g2m0; "
    f"{G} -DUNWRAP_PROBE -DGATE_POW2 -DUNWRAP_MODE=2 -o ../bin/g2m2 d14.cpp || echo BUILDFAIL_g2m2; "
    f"{G} -DUNWRAP_PROBE -DUNWRAP_MODE=0 -o ../bin/g3m0 d14.cpp || echo BUILDFAIL_g3m0; "
    f"{G} -DUNWRAP_PROBE -DUNWRAP_MODE=2 -o ../bin/g3m2 d14.cpp || echo BUILDFAIL_g3m2; "
    "echo BUILT"
)
rc, out, err = run(build, timeout=5400)
print(out[-3000:])
print(err[-3000:])
if "BUILT" not in out or "BUILDFAIL" in out:
    sys.exit("build failed")

remote = r'''cd ~/divbench && python3 - <<'EOF'
import subprocess, os, hashlib, collections
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
cases = sorted(f[:-3] for f in os.listdir(IN) if f.endswith('.in'))
BINS = ['d13','g2m0','g2m2','g3m0','g3m2']
print('%-32s %s' % ('case', ' '.join('%-6s' % b for b in BINS[1:])))
allstat = collections.Counter()
for c in cases:
    hs, probes = {}, {}
    for b in BINS:
        with open(IN+c+'.in','rb') as f:
            p = subprocess.run(['./bin/'+b], stdin=f, capture_output=True)
        hs[b] = hashlib.md5(p.stdout).hexdigest()
        probes[b] = p.stderr.decode('utf-8','replace')
    flags = []
    for b in BINS[1:]:
        flags.append('OK' if hs[b]==hs['d13'] else 'BAD')
    # 统计 g3m0 的 (cx,cy)
    st = collections.Counter()
    for ln in probes['g2m0'].splitlines():
        if ln.startswith('[unwrap]'):
            d = dict(kv.split('=') for kv in ln.split()[1:])
            st['g2 cx%s cy%s' % (d['cx'], d['cy'])] += 1
    for ln in probes['g3m0'].splitlines():
        if ln.startswith('[unwrap]'):
            d = dict(kv.split('=') for kv in ln.split()[1:])
            st['g3 cx%s cy%s' % (d['cx'], d['cy'])] += 1
    allstat.update(st)
    extra = ' '.join('%s:%d' % (k,v) for k,v in sorted(st.items())) if st else ''
    print('%-32s %s  %s' % (c, ' '.join('%-6s' % f for f in flags), extra))
    if flags[0]=='BAD' or flags[2]=='BAD':
        for ln in probes['g2m0'].splitlines()[:6]:
            print('      g2m0| '+ln)
        for ln in probes['g3m0'].splitlines()[:6]:
            print('      g3m0| '+ln)
print()
print('== total (cx,cy) ==')
for k,v in sorted(allstat.items()):
    print('  %s : %d' % (k,v))
EOF'''
rc, out, err = run(remote, timeout=3600)
print(out)
print(err[-2000:])

with open(os.path.join(PS, "lc_bench", "vm", "_probe_d14.log"), "w", encoding="utf-8") as f:
    f.write(out)
