#!/bin/bash
# D39 vs D41 全 26 例 callgrind 仲裁 (Zen3 cache 模型, est = Ir + 5*D1m + 200*DLm)
cd /home/azzr/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
CACHE="--cache-sim=yes --D1=32768,8,64 --I1=32768,8,64 --LL=33554432,16,64"

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

g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d39 src/div_D39.cpp 2>/tmp/d39.cerr && echo "BUILT d39" || { echo "BUILD FAIL d39"; head -20 /tmp/d39.cerr; }
g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d41 src/div_D41.cpp 2>/tmp/d41.cerr && echo "BUILT d41" || { echo "BUILD FAIL d41"; head -20 /tmp/d41.cerr; }

printf "%-30s %14s %14s %8s\n" case D39 D41 ratio
for f in $IN/*.in; do
  c=$(basename $f .in)
  valgrind --tool=callgrind $CACHE --callgrind-out-file=/tmp/c39.out ./bin/d39 < $f > /tmp/o39.txt 2>/dev/null
  A=$(python3 /tmp/cgsum.py /tmp/c39.out)
  valgrind --tool=callgrind $CACHE --callgrind-out-file=/tmp/c41.out ./bin/d41 < $f > /tmp/o41.txt 2>/dev/null
  B=$(python3 /tmp/cgsum.py /tmp/c41.out)
  if cmp -s /tmp/o39.txt /tmp/o41.txt; then D=""; else D="  <<< OUTPUT DIFF!"; fi
  printf "%-30s %14s %14s %8s%s\n" $c $A $B \
      "$(python3 -c "print(f'{$B/$A:.4f}')")" "$D"
done
echo ALLDONE
