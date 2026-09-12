import re, sys

def parse(path):
    fl = None
    fn = None
    cur = None
    cost = {}
    cost_re = re.compile(r'^((?:\*)|(?:[+-]?\d+)|(?:0x[0-9a-fA-F]+))\s+(\d+)')
    with open(path, errors='replace') as f:
        for line in f:
            s = line.rstrip('\n')
            if s.startswith('fl='):
                fl = s[3:].strip()
                cur = None
            elif s.startswith('fn='):
                fn = s[3:].strip() or '(anon)'
            elif s.startswith('cfn=') or s.startswith('calls='):
                pass
            elif s.startswith('*'):
                continue
            else:
                m = cost_re.match(s)
                if not m:
                    continue
                tok = m.group(1)
                ir = int(m.group(2))
                if tok.startswith('0x'):
                    key = (fl, fn, 'addr')
                    cost[key] = cost.get(key, 0) + ir
                elif tok.startswith('+'):
                    if cur is not None:
                        cur += int(tok)
                        cost[(fl, cur)] = cost.get((fl, cur), 0) + ir
                elif tok.startswith('-'):
                    if cur is not None:
                        cur -= int(tok[1:])
                        cost[(fl, cur)] = cost.get((fl, cur), 0) + ir
                else:
                    cur = int(tok)
                    cost[(fl, cur)] = cost.get((fl, cur), 0) + ir
    return cost

if __name__ == '__main__':
    path = sys.argv[1]
    filt = sys.argv[2] if len(sys.argv) > 2 else None
    src_local = sys.argv[3] if len(sys.argv) > 3 else None
    cost = parse(path)
    items = sorted(cost.items(), key=lambda kv: kv[1], reverse=True)
    if filt:
        items = [((fl, ln), v) for (fl, ln), v in items
                 if isinstance(fl, str) and filt in fl]
    total_src = sum(v for (_, v) in items)
    print(f"{'file:line':<42}{'Ir':>12}{'%':>7}   source")
    src_lines = {}
    if src_local:
        try:
            with open(src_local, errors='replace') as sf:
                src_lines = {i + 1: l.rstrip('\n') for i, l in enumerate(sf)}
        except Exception as e:
            print(f"(src read fail: {e})")
    for (fl, ln), v in items[:45]:
        src = ''
        if src_local and isinstance(ln, int) and ln in src_lines:
            src = src_lines[ln].strip()
            if len(src) > 60:
                src = src[:60] + '...'
        label = fl.split('/')[-1] if isinstance(fl, str) else str(fl)
        if isinstance(ln, int):
            label += f':{ln}'
        else:
            label += f'::{ln}'
        print(f"{label[:41]:<42}{v:>12}{100.0*v/total_src:>6.2f}%   {src}")
    print(f"--- filtered total Ir = {total_src} ---")
