#!/usr/bin/env python3
# 编译 -S 生成汇编, 按函数统计 ymm(256b) vs xmm(128b) 使用比例
# 目的: 判定 FFT 蝶形 / 点乘阶段是否只用了一半 SIMD 宽度
import subprocess, os, sys, re, collections

REMOTE = '/home/azzr/divbench'
SRC = sys.argv[1] if len(sys.argv) > 1 else 'div_D16'
os.chdir(REMOTE)

asm = '/tmp/%s.s' % SRC
cmd = ('g++ -O2 -std=c++23 -march=x86-64-v3 -S -o %s src/%s.cpp' % (asm, SRC))
r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
if r.returncode != 0:
    print('COMPILE FAIL'); print(r.stderr[-3000:]); sys.exit(1)

# 切分函数体
funcs = {}
cur = None
body = []
for ln in open(asm, encoding='utf-8', errors='replace'):
    m = re.match(r'^\s*\.type\s+([\w\.\$]+),\s*@function', ln)
    if m:
        cur = m.group(1); body = []; continue
    if cur is not None:
        m2 = re.match(r'^\s*\.size\s+([\w\.\$]+),', ln)
        if m2 and m2.group(1) == cur:
            funcs[cur] = body; cur = None; continue
        body.append(ln)

def demangle(names):
    p = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True)
    return p.stdout.splitlines()

# 关心的热点(按 _attr_d16_lri03.log 排序)
KEYS = ['dif', 'idit', 'dot_rfftX2', 'real_dot_binrev', 'dif3Stage', 'idit3Stage',
        'absMul1', 'fftMulPre', 'fftMulModBm1Pre', 'copyU16ToF64', 'absSub', 'absAdd']

rows = []
allnames = list(funcs.keys())
dem = demangle(allnames)
name2dem = dict(zip(allnames, dem))

for name, body in funcs.items():
    d = name2dem.get(name, name)
    ymm = xmm = tot = fma = 0
    for ln in body:
        s = ln.strip()
        if not s or s.startswith('.') or s.startswith('#') or s.endswith(':'):
            continue
        tot += 1
        has_y = '%ymm' in s
        has_x = '%xmm' in s
        if has_y: ymm += 1
        elif has_x: xmm += 1
        if s.startswith('vfmadd') or s.startswith('vfmsub') or s.startswith('vfnm'):
            fma += 1
    if tot < 40:
        continue
    rows.append((tot, ymm, xmm, fma, name, d))

rows.sort(reverse=True)
print('%-6s %-6s %-6s %-5s  %s' % ('TOT', 'YMM', 'XMM', 'FMA', 'FUNC'))
print('-' * 110)
shown = 0
for tot, ymm, xmm, fma, name, d in rows:
    if not any(k in d for k in KEYS):
        continue
    simd = ymm + xmm
    pct = (100.0 * ymm / simd) if simd else 0.0
    short = d
    if len(short) > 78:
        short = short[:75] + '...'
    print('%-6d %-6d %-6d %-5d  ymm%%=%5.1f  %s' % (tot, ymm, xmm, fma, pct, short))
    shown += 1
    if shown >= 40:
        break

print()
print('===== 全局汇总 (整个 TU) =====')
gy = gx = gt = gf = 0
for tot, ymm, xmm, fma, name, d in rows:
    gy += ymm; gx += xmm; gt += tot; gf += fma
print('total instr=%d  ymm=%d  xmm=%d  fma=%d  ymm%%=%.1f' %
      (gt, gy, gx, gf, 100.0 * gy / max(1, gy + gx)))
