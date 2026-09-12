#!/bin/bash
DIR=/home/azzr/precious_speed/cases_hex
: > /tmp/vres.txt
for f in "$DIR"/*.in; do
  nm=$(basename "$f" .in)
  timeout 90 /tmp/opt_src < "$f" > /tmp/os.out 2>/dev/null; rs=$?
  timeout 90 /tmp/opt_xf  < "$f" > /tmp/ox.out 2>/dev/null; rx=$?
  if [ $rs -eq 124 ]; then echo "$nm SRC_TIMEOUT" >> /tmp/vres.txt; continue; fi
  if [ $rx -eq 124 ]; then echo "$nm XF_TIMEOUT"  >> /tmp/vres.txt; continue; fi
  if [ $rs -ne 0 ]; then echo "$nm SRC_CRASH rc=$rs" >> /tmp/vres.txt; continue; fi
  if [ $rx -ne 0 ]; then echo "$nm XF_CRASH rc=$rx"  >> /tmp/vres.txt; continue; fi
  if diff -q /tmp/os.out /tmp/ox.out >/dev/null; then echo "$nm SAME" >> /tmp/vres.txt; else echo "$nm DIFF" >> /tmp/vres.txt; fi
done
echo "ALLDONE" >> /tmp/vres.txt
