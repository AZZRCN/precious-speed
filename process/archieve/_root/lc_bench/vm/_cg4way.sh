#!/bin/bash
# 四方 callgrind 全量仲裁: d31L(LC 33ms, 带assert) / d31N(+NDEBUG) / d39(LC 35ms) / d41
# est = Ir + 5*D1m + 200*DLm   (Zen3 cache 模型)
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

echo "=== BUILD ==="
g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d31L src/div_D31L.cpp 2>/tmp/e1.txt && echo "OK d31L" || { echo "FAIL d31L"; head -20 /tmp/e1.txt; }
g++ -O2 -std=c++23 -march=x86-64-v3 -DNDEBUG -o bin/d31N src/div_D31L.cpp 2>/tmp/e2.txt && echo "OK d31N" || { echo "FAIL d31N"; head -20 /tmp/e2.txt; }
g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d39 src/div_D39.cpp 2>/tmp/e3.txt && echo "OK d39" || { echo "FAIL d39"; head -20 /tmp/e3.txt; }
g++ -O2 -std=c++23 -march=x86-64-v3 -o bin/d41 src/div_D41.cpp 2>/tmp/e4.txt && echo "OK d41" || { echo "FAIL d41"; head -20 /tmp/e4.txt; }

echo "=== CALLGRIND (est, lower=better) ==="
printf "%-28s %13s %13s %13s %13s | %7s %7s %7s\n" case d31L d31N d39 d41 N/L 39/N 41/N
for f in $IN/*.in; do
  c=$(basename $f .in)
  for v in d31L d31N d39 d41; do
    valgrind --tool=callgrind $CACHE --callgrind-out-file=/tmp/c_$v.out ./bin/$v < $f > /tmp/o_$v.txt 2>/dev/null
    eval "E_$v=\$(python3 /tmp/cgsum.py /tmp/c_\$v.out)"
  done
  D=""
  cmp -s /tmp/o_d31L.txt /tmp/o_d31N.txt || D="$D [N!=L]"
  cmp -s /tmp/o_d31L.txt /tmp/o_d39.txt  || D="$D [39!=L]"
  cmp -s /tmp/o_d31L.txt /tmp/o_d41.txt  || D="$D [41!=L]"
  printf "%-28s %13s %13s %13s %13s | %7s %7s %7s%s\n" $c $E_d31L $E_d31N $E_d39 $E_d41 \
    "$(python3 -c "print(f'{$E_d31N/$E_d31L:.4f}')")" \
    "$(python3 -c "print(f'{$E_d39/$E_d31N:.4f}')")" \
    "$(python3 -c "print(f'{$E_d41/$E_d31N:.4f}')")" "$D"
done
echo ALLDONE
