#!/usr/bin/env python3
import os
p = r'd:\precious_speed\lc_bench\cases\mul'
for c in ['max_max_00 .in', 'max_max_06 .in', 'fft_killer_01.in']:
    f = os.path.join(p, c)
    if not os.path.exists(f):
        continue
    with open(f, 'r') as fh:
        lines = fh.read().split('\n')[:3]
    print(f'=== {c} ===')
    for i, line in enumerate(lines):
        parts = line.split(' ')
        p1 = parts[1][:50] if len(parts) > 1 else 'N/A'
        print(f'  line{i}: {len(line)} chars, parts={len(parts)}, p0={parts[0][:50]}, p1={p1}')
