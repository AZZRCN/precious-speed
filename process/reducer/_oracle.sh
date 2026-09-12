#!/bin/bash
fails=0; total=0
for f in /tmp/hexcmp/cases_dir/*.in; do
  total=$((total+1))
  ./ref < "$f" > /tmp/o_opt.txt 2>/dev/null
  rc_opt=$?
  ./ref < "$f" > /tmp/o_ref.txt 2>/dev/null
  rc_ref=$?
  if [ $rc_opt -ne 0 ] || [ $rc_ref -ne 0 ]; then fails=$((fails+1)); echo "CRASH $f opt=$rc_opt ref=$rc_ref"; continue; fi
  if ! cmp -s /tmp/o_ref.txt /tmp/o_opt.txt; then fails=$((fails+1)); echo "DIFF $f"; fi
done
echo "ORACLE_DONE total=$total fails=$fails"
