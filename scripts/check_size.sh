#!/bin/bash
ls -la /tmp/fusion/add_1M.in /tmp/fusion/mul_500k.in /tmp/fusion/div_1M_500k.in
echo "=== add input ==="
head -1 /tmp/fusion/add_1M.in
python3 -c "
with open('/tmp/fusion/add_1M.in') as f:
    n = int(f.readline())
    a = f.readline().strip()
    b = f.readline().strip()
    print('a digits:', len(a))
    print('b digits:', len(b))
    print('a limbs:', (len(a)+3)//4)
    print('b limbs:', (len(b)+3)//4)
"
