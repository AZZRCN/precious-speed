import sys

# Read moptm.cpp, extract lib part (lines 1-1773, 0-indexed 0-1772)
# Line 1773 is the #endif that closes #ifndef INTEGER_H
with open('moptm.cpp', 'r', encoding='utf-8') as f:
    lines = f.readlines()
lib = ''.join(lines[:1773])

# Read lc_add_moptm.cpp, extract I/O + main (after #include "moptm.cpp")
with open('lc_add_moptm.cpp', 'r', encoding='utf-8') as f:
    add_lines = f.readlines()
start = 0
for i, line in enumerate(add_lines):
    if '#include "moptm.cpp"' in line:
        start = i + 1
        break
io_main = ''.join(add_lines[start:])

# Write submit version
with open('lc_add_moptm_submit.cpp', 'w', encoding='utf-8', newline='\n') as f:
    f.write(lib)
    f.write('\n')
    f.write(io_main)

print(f'add submit: {len(lib.splitlines()) + len(io_main.splitlines())} lines')

# Same for mul
with open('lc_mul_moptm.cpp', 'r', encoding='utf-8') as f:
    mul_lines = f.readlines()
start = 0
for i, line in enumerate(mul_lines):
    if '#include "moptm.cpp"' in line:
        start = i + 1
        break
io_main_mul = ''.join(mul_lines[start:])

with open('lc_mul_moptm_submit.cpp', 'w', encoding='utf-8', newline='\n') as f:
    f.write(lib)
    f.write('\n')
    f.write(io_main_mul)

print(f'mul submit: {len(lib.splitlines()) + len(io_main_mul.splitlines())} lines')
