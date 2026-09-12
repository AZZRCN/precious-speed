#!/usr/bin/env python3
# 解析 callgrind.out 详细格式, 按 fn= 聚合 Ir, 输出 D45 vs D46 比值。
import re, sys

def parse(path):
    cur = "<global>"
    ir = {}
    fin = re.compile(r'^fn=\(\d+\)\s*(.+)$')
    cost = re.compile(r'^\d+(?:\s+\d+)*$')
    with open(path, errors='ignore') as f:
        for line in f:
            fm = fin.match(line)
            if fm:
                cur = fm.group(1).strip()
                continue
            s = line.strip()
            if cost.match(s):
                parts = s.split()
                ir[cur] = ir.get(cur, 0) + int(parts[0])
    return ir

a = parse(sys.argv[1])
b = parse(sys.argv[2])
allf = set(a) | set(b)
rows = []
for fn in allf:
    ia, ib = a.get(fn, 0), b.get(fn, 0)
    if ia == 0 and ib == 0:
        continue
    ratio = (ib / ia) if ia else float('inf')
    rows.append((fn, ia, ib, ratio))

# 只打印 Ir 较大的函数 (过滤极小噪音)
rows = [r for r in rows if max(r[1], r[2]) >= 1000]
rows.sort(key=lambda r: -max(r[1], r[2]))
print(f"{'func':42s} {'Ir45':>14s} {'Ir46':>14s} {'ratio46/45':>11s}")
for fn, ia, ib, ratio in rows:
    rs = f"{ratio:.4f}" if ratio != float('inf') else "new"
    print(f"{fn:42s} {ia:14d} {ib:14d} {rs:>11s}")
# 总计
ta, tb = sum(a.values()), sum(b.values())
print(f"\nTOTAL Ir45={ta}  Ir46={tb}  ratio={tb/ta:.4f}")
