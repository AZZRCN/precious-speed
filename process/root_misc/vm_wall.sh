#!/bin/bash
# wall-clock comparison (avg over repeats) for two binaries on one case
B1=$1; B2=$2; F=$3; R=${4:-15}
t() { local b=$1; local tot=0; for i in $(seq 1 $R); do local s=$(date +%s%N); "$b" < "$F" >/dev/null; local e=$(date +%s%N); tot=$((tot + e - s)); done; awk -v t=$tot -v r=$R 'BEGIN{printf "%.3f ms/run", t/r/1e6}'; }
echo "$B1: $(t $B1 $F $R)"
echo "$B2: $(t $B2 $F $R)"
