#!/bin/bash
cd /home/azzr/divbench
g++ -O2 -march=x86-64-v3 -std=c++23 -o v9_leaf10 /home/azzr/divbench/v9_leaf10.cpp 2>/tmp/c1.err
echo "RC_v9=$? $(tail -1 /tmp/c1.err)"
g++ -O2 -march=x86-64-v3 -std=c++23 -o hb_div /home/azzr/hexbench/div/div.cpp 2>/tmp/c2.err
echo "RC_hb=$? $(tail -1 /tmp/c2.err)"
for bin in v9_leaf10 hb_div; do
  ok=0; bad=0; badlist=""
  for f in /home/azzr/hexbench/data/div/*.in; do
    e=${f%.in}.exp
    if python3 hexcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); badlist="$badlist $(basename $f .in)"; fi
  done
  echo "OFFICIAL $bin: OK=$ok BAD=$bad$badlist"
done
