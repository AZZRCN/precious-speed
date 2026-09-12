import subprocess
import sys

# Run with cyclic disabled to get correct output
exe = r'tester/cur_div.exe'
with open(r'tester/failures/div_div_medium_98_0.in', 'rb') as f:
    in_data = f.read()
proc = subprocess.Popen(exe, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
out_data, _ = proc.communicate(in_data, timeout=30)

# Clean CR
out_clean = bytes(b for b in out_data if b != 13)
print(f'Output length: {len(out_clean)}')

# Find which test case has byte 631879
lines = out_clean.split(b'\n')
print(f'Number of lines: {len(lines)}')

pos = 0
for i, line in enumerate(lines):
    next_pos = pos + len(line) + 1  # +1 for newline
    if pos <= 631879 < next_pos:
        print(f'Byte 631879 is in line {i}, offset {631879 - pos}')
        print(f'Line content (first 100 bytes): {line[:100]}')
        # Determine if it's quotient or remainder
        # Line 0 = count, Line 1 = quotient of case 0, Line 2 = remainder of case 0
        # Line 3 = quotient of case 1, Line 4 = remainder of case 1
        if i == 0:
            print('This is the count line')
        else:
            case_idx = (i - 1) // 2
            is_remainder = (i - 1) % 2 == 1
            print(f'This is case {case_idx}, {"remainder" if is_remainder else "quotient"}')
        break
    pos = next_pos