#!/bin/bash
cd /home/azzr/hexbench
echo "=== data/mul listing ==="
ls data/mul/ | head -10
echo "=== expected file probe ==="
ls -la data/mul/example_00.* 2>/dev/null
echo "=== run L11 vs L1 on example_00 ==="
/tmp/v9L11 < data/mul/example_00.in > /tmp/o11.txt
/tmp/v9L1  < data/mul/example_00.in > /tmp/o1.txt
echo "IN:"; head -c 120 data/mul/example_00.in
echo ""
echo "L11:"; head -c 120 /tmp/o11.txt
echo ""
echo "L1 :"; head -c 120 /tmp/o1.txt
echo ""
if [ -f data/mul/example_00.out ]; then echo "EXP:"; head -c 120 data/mul/example_00.out; echo ""; fi
echo "=== full sweep: compare each LEAF against L11 output (self-consistency) ==="
for L in 1 3 5 7 9; do
  ok=1
  for f in data/mul/*.in; do
    /tmp/v9L11 < "$f" > /tmp/ref.txt
    /tmp/v9L$L < "$f" > /tmp/cur.txt
    cmp -s /tmp/ref.txt /tmp/cur.txt || { ok=0; echo "L$L DIFF $f"; break; }
  done
  [ $ok = 1 ] && echo "L$L MATCHES_L11"
done
