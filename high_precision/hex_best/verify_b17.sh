#!/bin/bash
cd ~
echo "=== build b17 ==="
g++-15 -O3 -std=c++20 -march=znver3 -mtune=znver3 -w div_base16_b17.cpp -o div_b17.bin 2>&1 | tail -5
echo BUILD_DONE
echo "=== smoke: first 3 small cases ==="
i=0
for f in /tmp/lccases/*.in; do
  name=$(basename "$f" .in)
  ./div_base16.bin < "$f" > /tmp/ref.out 2>/dev/null
  t0=$(date +%s)
  timeout 60 ./div_b17.bin < "$f" > /tmp/b17.out 2>/dev/null
  rc=$?
  t1=$(date +%s)
  dt=$((t1-t0))
  if [ $rc -eq 124 ]; then echo "$name: TIMEOUT(${dt}s)"; 
  elif cmp -s /tmp/ref.out /tmp/b17.out; then echo "$name: OK(${dt}s)";
  else echo "$name: MISMATCH(${dt}s)"; fi
  i=$((i+1)); [ $i -ge 3 ] && break
done
echo "=== full oracle with per-case timeout 150s ==="
fail=0
for f in /tmp/lccases/*.in; do
  name=$(basename "$f" .in)
  ./div_base16.bin < "$f" > /tmp/ref.out 2>/dev/null
  timeout 150 ./div_b17.bin < "$f" > /tmp/b17.out 2>/dev/null
  rc=$?
  if [ $rc -eq 124 ]; then echo "$name: TIMEOUT"; fail=1; continue; fi
  if ! cmp -s /tmp/ref.out /tmp/b17.out; then echo "$name: MISMATCH"; fail=1; else echo "$name: OK"; fi
done
[ $fail -eq 0 ] && echo "ORACLE_CLEAN" || echo "ORACLE_ISSUES"
echo "=== MAX point instructions:u (r3) ==="
echo -n "CANON_MAX: "
perf stat -e instructions:u -r 3 --log-fd 1 ./div_base16.bin < /tmp/lccases/length_ratio_integer_2.in 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1);print $1;exit}'
echo -n "B17_MAX:   "
perf stat -e instructions:u -r 3 --log-fd 1 ./div_b17.bin   < /tmp/lccases/length_ratio_integer_2.in 2>&1 | awk '/instructions:u/{gsub(/,/,"",$1);print $1;exit}'
