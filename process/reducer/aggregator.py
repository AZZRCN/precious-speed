import re, subprocess, sys

PATH = '/tmp/hexcmp/cg_np.out'
BIN = '/tmp/hexcmp/bin/ref_np'

lines = open(PATH, errors='replace').read().splitlines()

addr_re = re.compile(r'fn=\((\d+)\)\s+(0x[0-9a-f]+)')
id2addr = {}
addrs = set()
for ln in lines:
    m = addr_re.search(ln)
    if m:
        id2addr[m.group(1)] = m.group(2)
        addrs.add(m.group(2))
addrs = sorted(addrs)

sym = {}
if addrs:
    with open('/tmp/addrs.txt', 'w') as f:
        f.write('\n'.join(addrs))
    out = subprocess.check_output(['addr2line', '-e', BIN, '-f', '-C'] + addrs).decode(errors='replace')
    ol = out.splitlines()
    for k, a in enumerate(addrs):
        func = ol[2 * k] if 2 * k < len(ol) else '??'
        fl = ol[2 * k + 1] if 2 * k + 1 < len(ol) else '??'
        sym[a] = (func.strip(), fl.strip())

cost_re = re.compile(r'^((?:\*)|(?:[+-]?\d+)|(?:0x[0-9a-fA-F]+))\s+(\d+)')
fn_id = None
cur_name = None
in_cfn = False
cost = {}
order = []

def resolve(fn_id):
    if fn_id in id2addr and id2addr[fn_id] in sym:
        func, fl = sym[id2addr[fn_id]]
        if func and func != '??':
            return func
    return '(id %s)' % fn_id

for ln in lines:
    s = ln.rstrip('\n')
    if s.startswith('fn='):
        m = addr_re.search(s)
        if m:
            fn_id = m.group(1)
        else:
            mm = re.match(r'fn=\((\d+)\)', s)
            fn_id = mm.group(1) if mm else None
        cur_name = resolve(fn_id)
        in_cfn = False
        if cur_name not in cost:
            cost[cur_name] = 0
            order.append(cur_name)
    elif s.startswith('cfn='):
        in_cfn = True
    elif s.startswith('calls='):
        pass
    elif s.startswith('*'):
        continue
    else:
        m = cost_re.match(s)
        if m and cur_name is not None and not in_cfn:
            cost[cur_name] += int(m.group(2))

total = sum(cost.values())
items = sorted(cost.items(), key=lambda kv: kv[1], reverse=True)
print(f"TOTAL self Ir ~= {total}")
print(f"{'function':<48}{'self_Ir':>14}{'%':>7}")
for k, v in items[:45]:
    print(f"{k[:47]:<48}{v:>14}{100.0*v/total:>6.2f}%")
