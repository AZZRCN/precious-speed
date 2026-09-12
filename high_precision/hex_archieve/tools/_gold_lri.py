import sys
data = open('/tmp/lri1.in').read().split()
T = int(data[0]); idx = 1; out = []
for _ in range(T):
    a = int(data[idx], 16); b = int(data[idx + 1], 16); idx += 2
    out.append(format(a // b, 'x') + ' ' + format(a % b, 'x'))
open('/tmp/lri1_gold.txt', 'w').write(chr(10).join(out) + chr(10))
