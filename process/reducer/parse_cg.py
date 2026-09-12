#!/usr/bin/env python3
"""Parse a callgrind.out file and aggregate Ir (instruction refs) per function.
Robust to the cg_annotate 3.26 header quirk. Callgrind format:
  events: Ir Dr Dw ...   (first column = Ir)
  fn=name                 (current function)
  fl=file                 (current file)
  cost lines: ' <addr> <Ir> <Dr> ...'  (whitespace-led, space-separated numbers)
We sum the first numeric column under each fn= across the whole file.
"""
import sys, re

def parse(path):
    fn = None
    totals = {}
    order = []
    in_arc = False  # inside a cfn= call arc (inclusive cost -> skip)
    # cost line: first token is a (relative) line number, optionally +/- prefixed,
    # followed by event counts; second token is Ir. e.g. "33 1 0 0 ..." or "+4 1 1"
    cost_re = re.compile(r'^([+-]?\d+)\s+(\d+)')
    with open(path, 'r', errors='replace') as f:
        for line in f:
            s = line.rstrip('\n')
            if s.startswith('fn='):
                fn = s[3:].strip()
                in_arc = False
                if fn not in totals:
                    totals[fn] = 0
                    order.append(fn)
            elif s.startswith('fe='):
                in_arc = False
            elif s.startswith('cfn='):
                in_arc = True
            elif s.startswith('summary:'):
                pass
            else:
                m = cost_re.match(s)
                if m and fn is not None and not in_arc:
                    ir = int(m.group(2))
                    totals[fn] += ir
    return totals, order

def main():
    path = sys.argv[1]
    totals, order = parse(path)
    items = sorted(totals.items(), key=lambda kv: kv[1], reverse=True)
    grand = sum(totals.values())
    print(f"# total Ir (sum of per-fn) = {grand:,}")
    print(f"# {'function':50} {'Ir':>15} {'%':>7}")
    for fn, ir in items[:50]:
        print(f"{fn[:50]:50} {ir:>15,} {100.0*ir/grand:>6.2f}%")

if __name__ == '__main__':
    main()
