#!/bin/bash
# INV_NEWTON_BASE_THRESHOLD 扫描: absInvNewton 基例阈值 (schoolbook <-> FFT 交叉点)
# 零正确性风险 (基例与 FFT 路径数学等价), 纯 -D 开关。
cd /home/azzr/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
CACHE="--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"
TS="16 24 32 48 64 96 128"

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
  g++ -O2 -std=c++23 -march=x86-64-v3 -DINV_NEWTON_BASE_THRESHOLD=$T \
      -o bin/nt$T src/div_D39.cpp 2>/tmp/nt$T.cerr && echo "BUILT $T" || echo "BUILD FAIL $T"
done

printf "%-30s" case
for T in $TS; do printf "%13s" "T=$T"; done
echo
for c in r_nearly_zero_00 r_nearly_zero_01 burnikel_ziegler_bound_00 a_max_b_random_01 length_ratio_integer_01; do
  printf "%-30s" $c
  for T in $TS; do
    if [ ! -x bin/nt$T ]; then printf "%13s" "-"; continue; fi
    valgrind --tool=callgrind $CACHE --callgrind-out-file=/tmp/ntc.out \
        ./bin/nt$T < $IN/$c.in > /dev/null 2>/dev/null
    printf "%13s" "$(python3 /tmp/cgsum.py /tmp/ntc.out)"
  done
  echo
done
echo ALLDONE
