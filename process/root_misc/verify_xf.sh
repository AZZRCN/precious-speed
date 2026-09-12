#!/bin/bash
# verify_xf.sh: run opt_src (baseline) and opt_xf (xh-reuse) on each LC case.
# Gate: only flag divergence when BASELINE succeeds but xf differs (or xf crashes while base ok).
BASE=/tmp/opt_src
XF=/tmp/opt_xf
cd /home/azzr/precious_speed/cases_hex
CASES="length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 \
       max_0 max_1 max_2 amax_0 amax_1 amax_2 large_0 large_1 \
       medium_0 medium_1 medium_2 small_0 bzbound_0 bzbound_1 bzbound_2 bzbound_3 power_0"
rm -f /tmp/xf_diff.txt
fail=0; skip=0; ok=0
for f in $CASES; do
  $BASE < "$f.in" > /tmp/b_$f.out 2>/dev/null
  rb=$?
  if [ $rb -ne 0 ]; then echo "$f SKIP(base_rc=$rb)"; skip=$((skip+1)); continue; fi
  $XF   < "$f.in" > /tmp/x_$f.out 2>/dev/null
  rx=$?
  if [ $rx -ne 0 ]; then echo "$f XF_CRASH(rc=$rx) while base ok"; fail=1; continue; fi
  if ! diff -q /tmp/b_$f.out /tmp/x_$f.out >/dev/null; then
    echo "$f OUTPUT_DIFF"; fail=1
    diff /tmp/b_$f.out /tmp/x_$f.out | head -4 >> /tmp/xf_diff.txt
  else
    ok=$((ok+1))
  fi
done
echo "ok=$ok skip=$skip fail=$fail"
if [ $fail -eq 0 ]; then echo "ALL_MATCH"; else echo "MISMATCH_FOUND"; cat /tmp/xf_diff.txt; fi
