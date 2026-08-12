#!/bin/bash
cd /home/azzr/hexbench
BIN=${1:-/tmp/v9L11}
CASE=${2:-data/mul/max_max_01.in}
echo "### bin=$BIN case=$CASE"
echo "--- basic ---"
taskset -c 3 perf stat -e cycles,instructions,task-clock,page-faults "$BIN" < "$CASE" > /dev/null
echo "--- tlb/cache ---"
taskset -c 3 perf stat -e dTLB-load-misses,dTLB-store-misses,L1-dcache-load-misses,LLC-loads,LLC-load-misses "$BIN" < "$CASE" > /dev/null
echo "--- fp ---"
taskset -c 3 perf stat -e fp_arith_inst_retired.256b_packed_double,fp_arith_inst_retired.128b_packed_double,fp_arith_inst_retired.scalar_double "$BIN" < "$CASE" > /dev/null
echo "--- stalls ---"
taskset -c 3 perf stat -e cycle_activity.stalls_total,cycle_activity.stalls_mem_any,cycle_activity.stalls_l2_miss,cycle_activity.stalls_l3_miss "$BIN" < "$CASE" > /dev/null
