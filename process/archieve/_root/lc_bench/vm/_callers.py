#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""列出指定函数(子串匹配)的所有调用者边与调用次数。
用法: python3 _callers.py <callgrind.out> <substr> [<substr2> ...]
"""
import re
import sys
import collections

f = sys.argv[1]
pats = sys.argv[2:]

names = {}
curfn = '?'
pend_cfn = None
calls = collections.Counter()
inclir = collections.Counter()
selfir = collections.Counter()
positions = ['line']
hdr = re.compile(r'^(fl|fi|fe|fn|cob|cfi|cfl|cfn|ob)=(?:\((\d+)\))?\s*(.*)$')
pend_calls = None

with open(f, encoding='utf-8', errors='replace') as fh:
    for ln in fh:
        ln = ln.rstrip('\n')
        if not ln:
            continue
        if ln.startswith('positions:'):
            positions = ln.split(':', 1)[1].split()
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
            elif kind == 'cfn':
                pend_cfn = nm
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
        continue

# second pass needed for calls= lines
pend_calls = None
curfn = '?'
pend_cfn = None
names = {}
calls.clear()
inclir.clear()
with open(f, encoding='utf-8', errors='replace') as fh:
    for ln in fh:
        ln = ln.rstrip('\n')
        if not ln:
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


def short(s, n=52):
    return s if len(s) <= n else s[:n - 3] + '...'


for p in pats:
    print('==== callers of *%s* ====' % p)
    tot = 0
    for (a, b), c in sorted(calls.items(), key=lambda kv: -kv[1]):
        if b and p in b:
            print('  %-10d calls  inclIr=%9.2fM   from %s' % (c, inclir[(a, b)] / 1e6, short(a)))
            tot += c
    print('  TOTAL calls = %d' % tot)
    print()
print('DONE')
