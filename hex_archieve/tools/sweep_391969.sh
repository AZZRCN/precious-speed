set -u
cd /home/azzr/divbench
echo "=== compile ==="
g++ -O2 -march=x86-64-v3 -std=c++23 -o B17 391969_mul.cpp 2>build17.err && echo B17_OK || { echo B17_FAIL; cat build17.err; }
g++ -O2 -march=x86-64-v3 -std=c++23 -o B27 v7_leaf10.cpp 2>build27.err && echo B27_OK || { echo B27_FAIL; cat build27.err; }

echo "=== gate (22 official) ==="
for bin in B17 B27; do
  ok=0; bad=0
  for f in /home/azzr/hexbench/data/mul/*.in; do
    e=${f%.in}.exp
    python3 mulcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
  done
  echo "GATE $bin OK=$ok BAD=$bad"
done

cg() { # $1=bin $2=case -> prints Irefs LLdmiss
  inf=/home/azzr/hexbench/data/mul/$2.in
  [ -f "$inf" ] || { echo "$1 $2 MISSING"; return; }
  valgrind --tool=cachegrind --cache-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64 \
    --cachegrind-out-file=/tmp/cg_${1}_${2}.out ./$1 < "$inf" >/dev/null 2>/tmp/cg_${1}_${2}.err
  IR=$(grep -E "I refs:" /tmp/cg_${1}_${2}.err | grep -oE "[0-9]+")
  LL=$(grep -E "LLd misses:" /tmp/cg_${1}_${2}.err | grep -oE "[0-9]+")
  echo "$1 $2 Irefs=$IR LLdmiss=$LL"
}

echo "=== DIFF: v7(27ms) vs 391969(17ms) on key cases ==="
for c in max_max_06 large_02 large_00; do
  cg B27 $c
  cg B17 $c
done

echo "=== LEAF sweep on 391969 (10/11/12) ==="
for L in 10 12; do
  sed "s/#define FFT_LEAF_LOG 11/#define FFT_LEAF_LOG $L/" 391969_mul.cpp > 391969_L$L.cpp
  g++ -O2 -march=x86-64-v3 -std=c++23 -o B17L$L 391969_L$L.cpp 2>/dev/null && echo "B17L$L built" || echo "B17L$L FAIL"
done
for L in 10 11 12; do
  bin=B17L$L; [ "$L" = 11 ] && bin=B17
  for c in max_max_06 large_02 max_max_05; do
    cg $bin $c
  done
done
echo "ALL_DONE"
