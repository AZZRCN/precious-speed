#!/bin/bash
# 快速 benchmark: 5 runs x 50 loops, 输出毫秒
cd /tmp/bench
BIN=$1
IN=$2
LOOPS=${3:-50}
RUNS=${4:-5}
echo "=== $BIN ($IN, $RUNS runs x $LOOPS loops, ms) ==="
for run in $(seq 1 $RUNS); do
    start=$(date +%s.%N)
    for i in $(seq 1 $LOOPS); do
        ./$BIN < $IN > /dev/null
    done
    end=$(date +%s.%N)
    elapsed=$(awk "BEGIN{print ($end - $start) * 1000 / $LOOPS}")
    echo "run $run: $elapsed ms/loop"
done
