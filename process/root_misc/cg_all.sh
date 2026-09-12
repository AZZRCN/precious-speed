#!/bin/bash
# cg_all.sh: run callgrind (I refs) on each LC case file, log to /tmp/cg_all.log
BIN=/tmp/opt_src
LOG=/tmp/cg_all.log
rm -f "$LOG"
cd /home/azzr/precious_speed/cases_hex
for f in length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 \
         max_0 max_1 max_2 amax_0 amax_1 amax_2 large_0 large_1 \
         medium_0 medium_1 medium_2 small_0 rnear_0 rnear_1 rnear_2 bzbound_0 bzbound_1 bzbound_2 bzbound_3 power_0; do
  ir=$(valgrind --tool=callgrind --cache-sim=no --collect-jumps=no "$BIN" < "$f.in" 2>/tmp/cg_stderr.txt >/dev/null; grep -oE "I +refs: +[0-9,]+" /tmp/cg_stderr.txt | grep -oE "[0-9,]+")
  echo "$f Ir=$ir" >> "$LOG"
  echo "$f -> $ir"
done
echo DONE_CG_ALL
