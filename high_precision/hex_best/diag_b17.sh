#!/bin/bash
cd ~
echo "=== map precision wall: b17 correctness by case size ==="
for name in small_0 small_2 medium_0 medium_2 max_0 max_2 length_ratio_integer_1 length_ratio_integer_2 a_max_b_random_2; do
  if [ ! -f /tmp/lccases/$name.in ]; then echo "$name: NO_FILE"; continue; fi
  ./div_base16.bin < /tmp/lccases/$name.in > /tmp/ref.out 2>/dev/null
  timeout 40 ./div_b17.bin < /tmp/lccases/$name.in > /tmp/b17.out 2>/tmp/b17.err
  rc=$?
  if [ $rc -eq 124 ]; then echo "$name: TIMEOUT(spin)";
  elif cmp -s /tmp/ref.out /tmp/b17.out; then echo "$name: OK";
  else echo "$name: MISMATCH err=$(head -c 120 /tmp/b17.err | tr '\n' ' ')"; fi
done
echo "=== instructions:u on a safe medium case (r3) ==="
echo -n "medium_2 CANON: "; perf stat -e instructions:u -r 3 --log-fd 1 ./div_base16.bin < /tmp/lccases/medium_2.in 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1);print $1;exit}'
echo -n "medium_2 B17:   "; perf stat -e instructions:u -r 3 --log-fd 1 ./div_b17.bin   < /tmp/lccases/medium_2.in 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1);print $1;exit}'
