import re, sys

def parse(path):
    fn = None
    self_cost = {}
    order = []
    cost_re = re.compile(r'^((?:\*)|(?:[+-]?\d+)|(?:0x[0-9a-fA-F]+))\s+(\d+)')
    with open(path, errors='replace') as f:
        for line in f:
            s = line.rstrip('\n')
            if s.startswith('fn='):
                fn = s[3:].strip() or '(anon)'
                if fn not in self_cost:
                    self_cost[fn] = 0
                    order.append(fn)
            elif s.startswith('cfn=') or s.startswith('calls='):
                pass
            elif s.startswith('*'):
                continue
            else:
                m = cost_re.match(s)
                if m and fn is not None:
                    # second token is Ir (events order: Ir Dr Dw ...)
                    self_cost[fn] += int(m.group(2))
    return self_cost, order

if __name__ == '__main__':
    path = sys.argv[1]
    fn_filter = sys.argv[2] if len(sys.argv) > 2 else None
    costs, order = parse(path)
    total = sum(costs.values())
    items = sorted(costs.items(), key=lambda kv: kv[1], reverse=True)
    if fn_filter:
        items = [(k, v) for k, v in items if fn_filter in k]
    print(f"TOTAL self Ir = {total}")
    print(f"{'fn':<40}{'self_Ir':>16}{'%':>8}")
    for k, v in items[:40]:
        print(f"{k[:39]:<40}{v:>16}{100.0*v/total:>7.2f}%")
