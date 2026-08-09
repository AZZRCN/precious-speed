#!/usr/bin/env python3
# 动态 ISA 宽度剖析: callgrind --dump-instr=yes 得逐指令 Ir, objdump 映射到助记符,
# 按函数聚合 "执行到的指令" 中 ymm(256b) / xmm(128b) / 标量 的占比。
import subprocess, os, sys, re, collections

REMOTE = '/home/azzr/divbench'
IN = '/home/azzr/lcp/big_integer/division_of_big_integers/in/'
SRC = sys.argv[1] if len(sys.argv) > 1 else 'div_D16'
CASE = sys.argv[2] if len(sys.argv) > 2 else 'length_ratio_integer_03'
os.chdir(REMOTE)
os.makedirs('bin', exist_ok=True)

tag = 'ip_' + SRC
binp = 'bin/' + tag
cmd = 'g++ -O2 -g -std=c++23 -march=x86-64-v3 -o %s src/%s.cpp' % (binp, SRC)
r = subprocess.run(cmd, shell=True, capture_output=True, text=True)
if r.returncode != 0:
    print('COMPILE FAIL'); print(r.stderr[-3000:]); sys.exit(1)

outf = '/tmp/isa_%s_%s.out' % (SRC, CASE)
if not os.path.exists(outf):
    cg = ('valgrind --tool=callgrind --dump-instr=yes --collect-jumps=no '
          '--callgrind-out-file=%s %s < %s%s.in >/dev/null 2>/dev/null' % (outf, binp, IN, CASE))
    if subprocess.run(cg, shell=True).returncode != 0:
        print('RUN FAIL'); sys.exit(1)

# ---------------- 正确解析 callgrind 格式 ----------------
names = {}          # (kind, id) -> name
cost = collections.defaultdict(int)   # (fn, addr) -> Ir
positions = ['line']
curfn = '?'
pos_state = {}
in_call = False

hdr_re = re.compile(r'^(fl|fi|fe|fn|cob|cfi|cfl|cfn|ob)=(?:\((\d+)\))?\s*(.*)$')

with open(outf, encoding='utf-8', errors='replace') as f:
    for ln in f:
        ln = ln.rstrip('\n')
        if not ln:
            continue
        if ln.startswith('positions:'):
            positions = ln.split(':', 1)[1].split()
            pos_state = {p: 0 for p in positions}
            continue
        if ln.startswith(('events:', 'version:', 'creator:', 'cmd:', 'part:',
                          'desc:', 'pid:', 'summary:', 'totals:')):
            continue
        m = hdr_re.match(ln)
        if m:
            kind, cid, nm = m.group(1), m.group(2), m.group(3)
            if cid is not None:
                key = (kind if kind in ('fn', 'cfn') else kind, cid)
                # fn 与 cfn 共用同一命名空间
                nk = 'fn' if kind in ('fn', 'cfn') else kind
                key = (nk, cid)
                if nm:
                    names[key] = nm
                nm = names.get(key, cid)
            if kind == 'fn':
                curfn = nm
                pos_state = {p: 0 for p in positions}
            in_call = kind.startswith('c')
            continue
        if ln.startswith('calls='):
            in_call = True
            continue
        if ln[0] not in '0123456789+-*':
            continue
        parts = ln.split()
        npos = len(positions)
        if len(parts) < npos + 1:
            continue
        vals = {}
        ok = True
        for i, p in enumerate(positions):
            a = parts[i]
            try:
                if a == '*':
                    v = pos_state[p]
                elif a.startswith('0x') or a.startswith('0X'):
                    v = int(a, 16)
                elif a[0] == '+':
                    v = pos_state[p] + int(a[1:])
                elif a[0] == '-':
                    v = pos_state[p] - int(a[1:])
                else:
                    v = int(a)
            except ValueError:
                ok = False
                break
            pos_state[p] = v
            vals[p] = v
        if not ok:
            continue
        # calls= 之后的第一行是被调用者的归属代价, 不算本函数自身
        if in_call:
            in_call = False
            continue
        try:
            ir = int(parts[npos])
        except ValueError:
            continue
        addr = vals.get('instr')
        if addr is None:
            continue
        cost[(curfn, addr)] += ir

# --- objdump 反汇编映射 ---
dis = {}
p = subprocess.run(['objdump', '-d', '--no-show-raw-insn', binp],
                   capture_output=True, text=True)
for ln in p.stdout.splitlines():
    m = re.match(r'^\s*([0-9a-f]+):\s+(\S+)\s*(.*)$', ln)
    if m:
        dis[int(m.group(1), 16)] = (m.group(2), m.group(3))


def classify(mn, ops):
    if '%ymm' in ops or '%zmm' in ops:
        return 'v256'
    if '%xmm' in ops:
        if re.search(r'(sd|ss)$', mn) or re.search(r'(sd|ss)\b', mn):
            return 'fscalar'
        return 'v128'
    return 'scalar'


agg = collections.defaultdict(collections.Counter)
unknown = 0
for (fn, addr), ir in cost.items():
    d = dis.get(addr)
    if d is None:
        unknown += ir
        continue
    agg[fn][classify(*d)] += ir

rows = sorted(((sum(c.values()), fn, c) for fn, c in agg.items()), reverse=True)
total_all = sum(r[0] for r in rows) + unknown

print('==== 动态 ISA 宽度剖析: %s / %s ====' % (SRC, CASE))
print('total mapped Ir = %.2fM ; unmapped = %.2fM' % ((total_all - unknown) / 1e6, unknown / 1e6))
print('%-10s %-7s %-7s %-7s %-7s  %s' % ('Ir(M)', 'v256%', 'v128%', 'fscal%', 'scal%', 'FUNC'))
print('-' * 116)
for tot, fn, c in rows[:26]:
    short = fn if len(fn) <= 62 else fn[:59] + '...'
    print('%-10.2f %-7.1f %-7.1f %-7.1f %-7.1f  %s' %
          (tot / 1e6, 100.0 * c['v256'] / tot, 100.0 * c['v128'] / tot,
           100.0 * c['fscalar'] / tot, 100.0 * c['scalar'] / tot, short))

print()
BUT = ['FFT<double>::dif', 'FFT<double>::idit', 'dif3Stage', 'idit3Stage']
groups = {
    '蝶形族': BUT,
    '点乘族': ['dot_rfftX2', 'real_dot_binrev'],
    'FFT胶水': ['fftMulPre', 'fftMulModBm1Pre', 'copyU16ToF64', 'fftMul('],
}
for gname, keys in groups.items():
    bt = collections.Counter()
    for fn, c in agg.items():
        if any(k in fn for k in keys):
            bt.update(c)
    b = sum(bt.values())
    if not b:
        continue
    print('%s Ir=%.2fM  v256=%.1f%%  v128=%.1f%%  fscalar=%.1f%%  scalar=%.1f%%'
          % (gname, b / 1e6, 100.0 * bt['v256'] / b, 100.0 * bt['v128'] / b,
             100.0 * bt['fscalar'] / b, 100.0 * bt['scalar'] / b))
    print('   若 v128+fscalar 全部升到 v256 (理想上界): 省 Ir=%.2fM'
          % ((bt['v128'] + bt['fscalar'] * 0.75) / 2e6))

mn_agg = collections.Counter()
per_fn_mn = collections.defaultdict(collections.Counter)
for (fn, addr), ir in cost.items():
    d = dis.get(addr)
    if d:
        mn_agg[d[0]] += ir
        per_fn_mn[fn][d[0]] += ir
print()
print('==== 全局最热助记符 TOP25 ====')
tot_all = sum(mn_agg.values())
for mn, ir in mn_agg.most_common(25):
    print('  %-16s %8.2fM  %5.2f%%' % (mn, ir / 1e6, 100.0 * ir / tot_all))

# --- 分类: 算术 vs 搬运/置换 vs 标量开销 ---
ARITH = re.compile(r'^v?(add|sub|mul|div|fmadd|fmsub|fnmadd|fnmsub|addsub|sqrt|xor|and|or)'
                   r'(p[sd]|s[sd])?')
MOVE = re.compile(r'^v?(mov[au]p[sd]|movup[sd]|movap[sd]|movsd|movss|movddup|broadcast)')
SHUF = re.compile(r'^v?(perm|shuf|blend|unpck|insert|extract|pack|punpck)')
cat = collections.Counter()
for mn, ir in mn_agg.items():
    if SHUF.match(mn):
        cat['shuffle'] += ir
    elif MOVE.match(mn):
        cat['move'] += ir
    elif ARITH.match(mn) and ('p' in mn[-2:] or 's' in mn[-2:]):
        cat['arith'] += ir
    else:
        cat['other/scalar'] += ir
tt = sum(cat.values())
print()
print('==== 指令类别聚合 ====')
for k in ['arith', 'move', 'shuffle', 'other/scalar']:
    print('  %-14s %8.2fM  %5.2f%%' % (k, cat[k] / 1e6, 100.0 * cat[k] / tt))
print('  搬运+置换 / 算术 = %.2f' % ((cat['move'] + cat['shuffle']) / max(1, cat['arith'])))

print()
print('==== 热点函数逐助记符 (TOP8 函数 x TOP12 助记符) ====')
for tot, fn, c in rows[:8]:
    short = fn if len(fn) <= 70 else fn[:67] + '...'
    print('--- %.2fM  %s' % (tot / 1e6, short))
    for mn, ir in per_fn_mn[fn].most_common(12):
        print('      %-16s %7.2fM  %5.1f%%' % (mn, ir / 1e6, 100.0 * ir / tot))
