#!/bin/bash
# HEX division comparison: original #393027 (v11) vs optimized v27
# Wall-clock: 11 rounds -> median/min/mean. callgrind --cache-sim: I refs + D1/LL miss (big cats).
set -u
ROOT=/tmp/hexcmp
PROB=$ROOT/prob
BIN=$ROOT/bin
GENIN=$ROOT/gen_in
CG=$ROOT/cg
mkdir -p $BIN $GENIN $CG

echo "==== MACHINE ===="
g++ --version | head -1
grep -m1 "model name" /proc/cpuinfo
echo "=================="

cp $ROOT/random.h $PROB/gen/random.h
cd $PROB/gen
for g in small medium large max a_max_b_random power r_nearly_zero length_ratio_integer burnikel_ziegler_bound; do
  if g++ -O2 -std=c++20 -I.. -I. $g.cpp -o $BIN/gen_$g 2> $ROOT/err_gen_$g; then echo "GEN OK   $g"; else echo "GEN FAIL $g"; fi
done
g++ -O3 -std=c++20 -march=znver3 -mtune=znver3 -w $ROOT/orig.cpp -o $BIN/orig 2> $ROOT/err_orig && echo "SOL OK   orig" || echo "SOL FAIL orig"
g++ -O3 -std=c++20 -march=znver3 -mtune=znver3 -w $ROOT/opt.cpp  -o $BIN/opt  2> $ROOT/err_opt  && echo "SOL OK   opt"  || echo "SOL FAIL opt"

CATS="small medium large max a_max_b_random power r_nearly_zero length_ratio_integer burnikel_ziegler_bound"
declare -A SEED=( [small]=1 [medium]=1 [large]=1 [max]=3 [a_max_b_random]=1 [power]=1 [r_nearly_zero]=1 [length_ratio_integer]=5 [burnikel_ziegler_bound]=1 )
BIG="max a_max_b_random length_ratio_integer burnikel_ziegler_bound"

stats() {
  # reads newline-separated ns; prints "med min mean"
  sort -n | awk '{a[NR]=$1; s+=$1} END{
    n=NR;
    if(n%2==1) med=a[(n+1)/2]; else med=(a[n/2]+a[n/2+1])/2;
    printf "%.0f %.0f %.0f", med, a[1], s/n
  }'
}

echo -e "category\tseed\torig_med_ms\torig_min_ms\torig_mean_ms\topt_med_ms\topt_min_ms\topt_mean_ms\tspeedup_med\torig_Ir\topt_Ir\tIr_ratio\torig_D1mr\topt_D1mr\torig_DLmr\topt_DLmr" > $ROOT/results.tsv

for cat in $CATS; do
  seed=${SEED[$cat]}
  [ -x $BIN/gen_$cat ] || { echo "SKIP $cat"; continue; }
  $BIN/gen_$cat $seed > $GENIN/$cat.in
  IN=$GENIN/$cat.in
  $BIN/orig < $IN > $ROOT/o_orig.txt 2>/dev/null
  $BIN/opt  < $IN > $ROOT/o_opt.txt  2>/dev/null
  if diff -q $ROOT/o_orig.txt $ROOT/o_opt.txt >/dev/null; then echo "AGREE    $cat"; else echo "MISMATCH $cat !!!"; fi
  for sol in orig opt; do
    for i in $(seq 1 11); do
      s=$(date +%s%N); $BIN/$sol < $IN > /dev/null 2>&1; e=$(date +%s%N);
      echo $((e-s)) >> $ROOT/wt_${cat}_${sol}.txt
    done
    if echo "$BIG" | grep -qw "$cat"; then
      valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=$CG/cg_${cat}_${sol}.out $BIN/$sol < $IN > /dev/null 2>&1
    fi
  done
  read om omin omean <<< "$(stats < $ROOT/wt_${cat}_orig.txt)"
  read pm pmin pmean <<< "$(stats < $ROOT/wt_${cat}_opt.txt)"
  sp=$(python3 -c "print('%.2f'%(($om/$pm) if $pm>0 else 0))")
  parse_cg() {
    f=$1; [ -f "$f" ] || { echo -e "-\t-\t-\t-\t-"; return; }
    python3 - "$f" <<'PY'
import sys
d={}
for ln in open(sys.argv[1]):
    if ln.startswith("events:"): ev=ln.split()[1:]
    elif ln.startswith("summary:"): sm=[int(x) for x in ln.split()[1:]]
if 'ev' in dir() and 'sm' in dir():
    dd=dict(zip(ev,sm)); print(dd.get("Ir",0),dd.get("D1mr",0),dd.get("DLmr",0),dd.get("D1mw",0),dd.get("DLmw",0))
else: print("-","-","-","-","-")
PY
  }
  if echo "$BIG" | grep -qw "$cat"; then
    read oIr oD1 oDL oD1w oDLw <<< "$(parse_cg $CG/cg_${cat}_orig.out)"
    read pIr pD1 pDL pD1w pDLw <<< "$(parse_cg $CG/cg_${cat}_opt.out)"
    irrat=$(python3 -c "print('%.3f'%(($oIr/$pIr) if $pIr>0 else 0))")
    echo -e "$cat\t$seed\t$om\t$omin\t$omean\t$pm\t$pmin\t$pmean\t$sp\t$oIr\t$pIr\t$irrat\t$oD1\t$pD1\t$oDL\t$pDL" >> $ROOT/results.tsv
  else
    echo -e "$cat\t$seed\t$om\t$omin\t$omean\t$pm\t$pmin\t$pmean\t$sp\t-\t-\t-\t-\t-\t-\t-" >> $ROOT/results.tsv
  fi
  rm -f $ROOT/wt_${cat}_orig.txt $ROOT/wt_${cat}_opt.txt
done

echo "==== RESULTS (ms; I refs; D1mr=LLC? no, L1 D misses; DLmr=LL D misses) ===="
cat $ROOT/results.tsv
