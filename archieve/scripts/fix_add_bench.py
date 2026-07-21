#!/usr/bin/env python3
# fix_add_bench.py
# 修复 ADD main 的计时输出
import sys

with open('/tmp/bench2/moptm_bench.cpp', 'r') as f:
    lines = f.readlines()

# 找到 ADD main 的 flushOutput 位置（后面跟 #ifdef PROFILE_DIV 的那个）
inserted = False
for i, line in enumerate(lines):
    if 'flushOutput();' in line and i+1 < len(lines) and '#ifdef PROFILE_DIV' in lines[i+1]:
        insert_line = '    fprintf(stderr, "CPU: %.3f ms\\n", double(clock() - _bench_t0) * 1000.0 / CLOCKS_PER_SEC);\n'
        lines.insert(i+1, insert_line)
        print(f'Inserted at line {i+2}')
        inserted = True
        break

if not inserted:
    print('ERROR: ADD main flushOutput not found')
    sys.exit(1)

with open('/tmp/bench2/moptm_bench.cpp', 'w') as f:
    f.writelines(lines)

print("Done")
