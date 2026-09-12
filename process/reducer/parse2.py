#!/usr/bin/env python3
"""Robust callgrind exclusive-Ir per function. Handles leading whitespace on
cost lines and resets arc state on fn=/fe=. Debug mode for a target fn id."""
import sys, re

COST = re.compile(r'^\s*([+-]?\d+)\s+(\d+)')

def parse(path, debug_fn=None):
    fn = None
    totals = {}
    order = []
    in_arc = False
    dbg = []
    with open(path, errors='replace') as f:
        for raw in f:
            s = raw.rstrip('\n')
            if s.startswith('fn='):
                fn = s[3:].strip()
                in_arc = False
                if fn not in totals:
                    totals[fn] = 0; order.append(fn)
            elif s.startswith('fe='):
                in_arc = False
            elif s.startswith('cfn='):
                in_arc = True
            elif s.startswith('cob=') or s.startswith('cfi=') or s.startswith('fl=') or s.startswith('fi='):
                pass
            else:
                m = COST.match(s)
                if m and fn is not None:
                    ir = int(m.group(2))
                    if in_arc:
                        if debug_fn and fn == debug_fn:
                            dbg.append(('SKIP', s[:60], ir))
                    else:
                        totals[fn] += ir
                        if debug_fn and fn == debug_fn:
                            dbg.append(('ADD', s[:60], ir))
    return totals, order, dbg

if __name__ == '__main__':
    path = sys.argv[1]
    dbg = sys.argv[2] if len(sys.argv) > 2 else None
    totals, order, d = parse(path, dbg)
    items = sorted(totals.items(), key=lambda kv: kv[1], reverse=True)
    grand = sum(totals.values())
    print(f"# grand exclusive Ir sum = {grand:,}")
    for fn, ir in items[:45]:
        print(f"{fn[:46]:46} {ir:>15,} {100.0*ir/grand:>6.2f}%")
    if dbg:
        print("=== DEBUG", dbg, "===")
        for tag, line, ir in d[:20]:
            print(tag, ir, repr(line))
