#!/usr/bin/env python3
import random
random.seed(42)
t = 100
lines = [str(t)]
for _ in range(t):
    a_digits = random.randint(5000, 50000) * 4
    b_digits = random.randint(5000, a_digits // 2) * 4
    a = str(random.randint(5, 9)) + ''.join(str(random.randint(0, 9)) for _ in range(a_digits - 1))
    b = str(random.randint(5, 9)) + ''.join(str(random.randint(0, 9)) for _ in range(b_digits - 1))
    lines.append(a)
    lines.append(b)
with open('/tmp/div_fuzz_100.in', 'w', encoding='ascii', newline='\n') as f:
    f.write('\n'.join(lines) + '\n')
print(f"Generated {t} test cases to /tmp/div_fuzz_100.in")
