#!/usr/bin/env python3
import os, re, collections
CASES = ['length_ratio_integer_00']

def load(case):
    f = '/tmp/cgz_%s.out' % case
    if not os.path.exists(f):
        print('MISSING', f); return None
    names = {}          # id -> name  (fn / cfn 共用一个命名空间? 不, 分开)
    fnames, cfnames = {}, {}
    self_ir = collections.Counter()
    calls = collections.Counter()      # callee name -> total call count
    callee_ir = collections.Counter()  # callee name -> inclusive Ir from call sites
    cur_fn = None
    pend_calls = 0
    pend_cfn = None
    rx = re.compile(r'^\((\d+)\)\s*(.*)$')
    with open(f) as fh:
        for line in fh:
            line = line.rstrip('\n')
            if line.startswith('fn='):
                m = rx.match(line[3:])
                if m:
                    i, nm = m.group(1), m.group(2)
                    if nm: fnames[i] = nm
                    cur_fn = fnames.get(i, '?')
                else:
                    cur_fn = line[3:]
                pend_cfn = None
            elif line.startswith('cfn='):
                m = rx.match(line[4:])
                if m:
                    i, nm = m.group(1), m.group(2)
                    if nm: cfnames[i] = nm
                    pend_cfn = cfnames.get(i, '?')
                else:
                    pend_cfn = line[4:]
            elif line.startswith('calls='):
                pend_calls = int(line[6:].split()[0])
            elif line and (line[0].isdigit() or line[0] in '+-*'):
                parts = line.split()
                if len(parts) < 2: continue
                try: ir = int(parts[1])
                except ValueError: continue
                if pend_cfn is not None and pend_calls:
                    calls[pend_cfn] += pend_calls
                    callee_ir[pend_cfn] += ir
                    pend_cfn = None; pend_calls = 0
                elif cur_fn:
                    self_ir[cur_fn] += ir
    return self_ir, calls, callee_ir

for case in CASES:
    r = load(case)
    if not r: continue
    self_ir, calls, callee_ir = r
    tot = sum(self_ir.values()) or 1
    print('===== %s   total self Ir = %d' % (case, tot), flush=True)
    print('  %-46s %9s %13s %7s %12s' % ('function', 'calls', 'incl_Ir', 'share', 'Ir/call'), flush=True)
    rows = sorted(calls.items(), key=lambda kv: -callee_ir[kv[0]])
    for nm, c in rows[:26]:
        ii = callee_ir[nm]
        print('  %-46s %9d %13d %6.2f%% %12.1f'
              % (nm[:46], c, ii, 100.0*ii/tot, ii/c if c else 0), flush=True)
    print(flush=True)
print('DONE', flush=True)
