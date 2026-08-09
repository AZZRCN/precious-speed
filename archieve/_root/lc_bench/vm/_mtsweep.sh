#!/bin/bash
cd /home/azzr/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
CACHE="--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"
TS="64 96 128 192 256"

cat > /tmp/cgsum.py <<'PYEOF'
import sys
ev = tot = None
for line in open(sys.argv[1]):
    if line.startswith("events:"):
        ev = line.split()[1:]
    elif line.startswith("summary:") or line.startswith("totals:"):
        tot = [int(x) for x in line.split()[1:]]
d = dict(zip(ev, tot))
print(d.get("Ir", 0) + 5 * (d.get("D1mr", 0) + d.get("D1mw", 0))
      + 200 * (d.get("DLmr", 0) + d.get("DLmw", 0)))
PYEOF

for T in $TS; do
  g++ -O2 -std=c++23 -march=x86-64-v3 -DMUL_BASIC_THRESHOLD=$T -DSQR_BASIC_THRESHOLD=$T \
      -o bin/mt$T src/d39.cpp 2>/tmp/mt$T.cerr && echo "BUILT $T" || echo "BUILD FAIL $T"
done

printf "%-28s" case
for T in $TS; do printf "%14s" "T=$T"; done
echo
for c in burnikel_ziegler_bound_00 r_nearly_zero_01 a_max_b_random_01 length_ratio_integer_01; do
  printf "%-28s" $c
  for T in $TS; do
    valgrind --tool=callgrind $CACHE --callgrind-out-file=/tmp/mtc.out \
        ./bin/mt$T < $IN/$c.in > /dev/null 2>/dev/null
    printf "%14s" "$(python3 /tmp/cgsum.py /tmp/mtc.out)"
  done
  echo
done
echo ALLDONE
