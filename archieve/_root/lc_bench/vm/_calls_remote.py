#!/usr/bin/env python3
# 从已有 callgrind 输出提取函数调用次数 (calls=) 与被调用者归属代价
import re, sys, collections, os

f = sys.argv[1] if len(sys.argv) > 1 else '/tmp/isa_div_D16_length_ratio_integer_03.out'
names = {}
curfn = '?'
pend_cfn = None
calls = collections.Counter()          # (caller, callee) -> 调用次数
selfir = collections.Counter()
inclir = collections.Counter()         # (caller, callee) -> 归属 Ir
positions = ['line']
pos_state = {}
hdr = re.compile(r'^(fl|fi|fe|fn|cob|cfi|cfl|cfn|ob)=(?:\((\d+)\))?\s*(.*)$')
pend_calls = None

with open(f, encoding='utf-8', errors='replace') as fh:
    for ln in fh:
        ln = ln.rstrip('\n')
        if not ln:
            continue
        if ln.startswith('positions:'):
            positions = ln.split(':', 1)[1].split()
            pos_state = {p: 0 for p in positions}
            continue
        m = hdr.match(ln)
        if m:
            kind, cid, nm = m.group(1), m.group(2), m.group(3)
            nk = 'fn' if kind in ('fn', 'cfn') else kind
            if cid is not None:
                if nm:
                    names[(nk, cid)] = nm
                nm = names.get((nk, cid), cid)
            if kind == 'fn':
                curfn = nm
                pos_state = {p: 0 for p in positions}
            elif kind == 'cfn':
                pend_cfn = nm
            continue
        if ln.startswith('calls='):
            pend_calls = int(ln.split('=')[1].split()[0])
            continue
        if ln[0] not in '0123456789+-*':
            continue
        parts = ln.split()
        npos = len(positions)
        if len(parts) < npos + 1:
            continue
        try:
            ir = int(parts[npos])
        except ValueError:
            continue
        if pend_calls is not None:
            calls[(curfn, pend_cfn)] += pend_calls
            inclir[(curfn, pend_cfn)] += ir
            pend_calls = None
        else:
            selfir[curfn] += ir

def short(s, n=58):
    return s if len(s) <= n else s[:n - 3] + '...'

print('==== 调用次数 TOP25 (caller -> callee) ====')
print('%-12s %-14s  %s' % ('calls', 'inclIr(M)', 'edge'))
for (a, b), c in calls.most_common(25):
    print('%-12d %-14.2f  %s -> %s' % (c, inclir[(a, b)] / 1e6, short(a, 40), short(b, 40)))

print()
print('==== 每函数总被调用次数 TOP20 ====')
tot = collections.Counter()
for (a, b), c in calls.items():
    tot[b] += c
for b, c in tot.most_common(20):
    si = selfir.get(b, 0)
    print('  %-10d calls  selfIr=%8.2fM  avg=%7.1f Ir/call   %s'
          % (c, si / 1e6, si / max(1, c), short(b, 62)))
