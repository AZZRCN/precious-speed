#!/bin/bash
# 定位 B=1 形态 (984M 指令, 比其他形态贵 4-8x) 的热点
cd ~
python3 - <<'EOF'
import random
random.seed(0xBEEF)
def rh(nd):
    v = random.getrandbits(nd*4); s = format(v,'X')
    return s[:nd] if len(s)>=nd else s
A = rh(1600000); B = rh(1)
open('/tmp/b1.txt','w').write("1\n%s %s\n" % (A,B))
print("A_len=%d B=%s" % (len(A), B))
EOF
echo "=== instructions:u ==="
perf stat -e instructions:u -x, /tmp/dv_0_32 < /tmp/b1.txt 2>&1 >/dev/null | grep instr | cut -d, -f1
echo "=== hotspots (instructions:u sampling) ==="
perf record -q -e instructions:u -c 50000 -o /tmp/b1.data /tmp/dv_0_32 < /tmp/b1.txt >/dev/null 2>&1
perf report -i /tmp/b1.data --stdio --no-children -q --percent-limit 0.5 2>/dev/null | grep -E '^\s+[0-9]' | head -20
