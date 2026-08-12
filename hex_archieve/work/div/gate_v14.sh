#!/bin/bash
cd /home/azzr
ok=0; bad=0
: > gate_bad.txt
for f in hexbench/data/div/*.in; do
  e="${f%.in}.exp"
  r=$(timeout 60 python3 divbench/hexcheck.py run ./div_v14 "$f" "$e" 2>&1)
  rc=$?
  if echo "$r" | grep -q OK; then
    ok=$((ok+1))
  else
    bad=$((bad+1))
    echo "BAD $(basename "$f") rc=$rc :: $r" >> gate_bad.txt
  fi
done
echo "OK=$ok BAD=$bad" > gate_res.txt
cat gate_bad.txt >> gate_res.txt
