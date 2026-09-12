#!/bin/bash
# Final lock-in: verify authoritative submit_div.cpp / submit_mul.cpp + extended LEAF sweep 13-16
set -u
cd /home/azzr/divbench
DIVDATA=/home/azzr/hexbench/data/div
MULDATA=/home/azzr/hexbench/data/mul

echo "=== BUILD authoritative candidates ==="
g++ -O2 -march=native -std=c++23 -o SD submit_div.cpp 2>/tmp/d.err || { echo "DIV BUILD FAIL"; cat /tmp/d.err; exit 1; }
g++ -O2 -march=native -std=c++23 -o SM submit_mul.cpp 2>/tmp/m.err || { echo "MUL BUILD FAIL"; cat /tmp/m.err; exit 1; }
echo "BUILD OK"

gate() {
  local bin="$1" data="$2"
  local ok=0 bad=0 bl=""
  for f in "$data"/*.in; do
    e="${f%.in}.exp"
    if python3 hexcheck.py run "./$bin" "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); bl="$bl $(basename "$f" .in)"; fi
  done
  echo "GATE $bin OK=$ok BAD=$bad$bl"
}
echo "=== DIV GATE (27) ==="; gate SD "$DIVDATA"
echo "=== MUL GATE (22) ==="; gate SM "$MULDATA"

bestof() {  # bin inf trials
  local bin="$1" inf="$2" t="$3" b=999999 d s e
  for r in $(seq 1 $t); do
    s=$(date +%s%N)
    "./$bin" < "$inf" >/dev/null
    e=$(date +%s%N)
    d=$(( (e-s)/1000000 ))
    [ $d -lt $b ] && b=$d
  done
  echo $b
}
echo "=== WORST-CASE TIMING (idle best-of-5) ==="
for c in length_ratio_integer_00 a_max_b_random_02 max_00; do
  inf="$DIVDATA/$c.in"; [ -f "$inf" ] && echo "DIV $c SD=$(bestof SD $inf 5)ms"
done
for c in max_max_05 large_02; do
  inf="$MULDATA/$c.in"; [ -f "$inf" ] && echo "MUL $c SM=$(bestof SM $inf 5)ms"
done

echo "=== EXTENDED LEAF SWEEP 13-16 (compile -DFFT_LEAF_LOG=N) ==="
for N in 13 14 15 16; do
  g++ -O2 -march=native -std=c++23 -DFFT_LEAF_LOG=$N -o SDn submit_div.cpp 2>/dev/null && {
    echo "DIV LEAF=$N length_ratio_integer_00=$(bestof SDn $DIVDATA/length_ratio_integer_00.in 5)ms a_max_b_random_02=$(bestof SDn $DIVDATA/a_max_b_random_02.in 5)ms"
  } || echo "DIV LEAF=$N BUILD FAIL"
  g++ -O2 -march=native -std=c++23 -DFFT_LEAF_LOG=$N -o SMn submit_mul.cpp 2>/dev/null && {
    echo "MUL LEAF=$N max_max_05=$(bestof SMn $MULDATA/max_max_05.in 5)ms large_02=$(bestof SMn $MULDATA/large_02.in 5)ms"
  } || echo "MUL LEAF=$N BUILD FAIL"
done
echo FINAL_DONE
