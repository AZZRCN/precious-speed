#!/usr/bin/env bash
# Focused high-round wall-clock re-measure of the LONGEST test points.
# Improvements over hexcmp_run.sh:
#   - ABAB interleaving with odd/even order swap (cancels thermal / freq drift and order bias)
#   - 51 rounds (configurable via R=)
#   - reports min / p25 / median / p75 / max / mean / stdev + distribution-separation verdict
set -u
ROOT=/tmp/hexcmp
BIN=$ROOT/bin
GI=$ROOT/gen_in
R=${R:-51}
CATS=${CATS:-"a_max_b_random burnikel_ziegler_bound length_ratio_integer max"}

TASK=""
if command -v taskset >/dev/null 2>&1; then TASK="taskset -c 2"; fi

echo "rounds=$R  taskset='$TASK'  cats=$CATS"
echo "host=$(hostname)  cpu=$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ //')"
echo

for cat in $CATS; do
  IN=$GI/$cat.in
  [ -f "$IN" ] || { echo "MISSING $IN"; continue; }
  : > $ROOT/lp_${cat}_orig.txt
  : > $ROOT/lp_${cat}_opt.txt
  # warmup: 3 each, page-cache + branch predictor settle
  for w in 1 2 3; do
    $TASK $BIN/orig < $IN > /dev/null 2>&1
    $TASK $BIN/opt  < $IN > /dev/null 2>&1
  done
  for ((r=0;r<R;r++)); do
    if [ $((r%2)) -eq 0 ]; then ORDER="orig opt"; else ORDER="opt orig"; fi
    for s in $ORDER; do
      st=$(date +%s%N)
      $TASK $BIN/$s < $IN > /dev/null 2>&1
      en=$(date +%s%N)
      echo $((en-st)) >> $ROOT/lp_${cat}_${s}.txt
    done
  done
  echo "done $cat ($R rounds x2)"
done

echo
python3 $ROOT/longpoint_stat.py $ROOT $CATS
