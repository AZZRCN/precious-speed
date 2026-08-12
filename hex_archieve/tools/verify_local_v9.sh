cd /home/azzr/divbench
g++ -O2 -march=x86-64-v3 -std=c++23 -o v9_local_leaf10 v9_local_leaf10.cpp 2>/tmp/c1.err; echo "RC_compile=$?"
ok=0; bad=0; badlist=""
for f in /home/azzr/hexbench/data/div/*.in; do
  e=${f%.in}.exp
  if python3 hexcheck.py run ./v9_local_leaf10 "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); badlist="$badlist $(basename $f)"; fi
done
echo "LOCAL_V9_OFFICIAL: OK=$ok BAD=$bad$badlist"
echo "--- cachegrind Irefs (local v9 LEAF=10) ---"
for cf in max_00 large_00 a_max_b_random_02 bench_mid; do
  fin=/home/azzr/hexbench/data/div/${cf}.in
  ir=$(valgrind --tool=cachegrind --cache-sim=yes --branches=no --I1=32768,8,64 --D1=32768,8,64 --LL=524288,8,64 --LL2=33554432,16,64 --cg-out-file=/tmp/cg_${cf}.out ./v9_local_leaf10 < $fin >/dev/null 2>/dev/null; cg_annotate --auto=no --show=Irefs,LLd /tmp/cg_${cf}.out 2>/dev/null | grep -E "^(Irefs|LLd)" | head -2 | tr '\n' ' ')
  echo "local_v9 $cf :: $ir"
done
echo "VERIFY_LOCAL_V9_DONE"
