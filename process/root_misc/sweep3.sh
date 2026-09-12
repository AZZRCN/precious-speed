#!/bin/bash
# sweep3.sh: 修两处 bug 的干净版
#   - bytecmp: env 与 base 拆分写法, 避免 eval 边界; 通配 cases
#   - callgrind: tag 用干净名(BASE/CYCB49/CYCB50, 无 '='), 巨例 monster 并发(限3)
cd /tmp/hexcmp
export OMP_NUM_THREADS=1
exec > sweep3_out.txt 2>&1
ALL=$(ls cases/case_*.in 2>/dev/null)
ALL="$ALL max_eq.in max_r2.in max_r8.in monster.in"
echo "CASE_COUNT=$(echo $ALL | wc -w)"
# ---- bytecmp 全变体 vs 默认 ref_base ----
for spec in "BASE|" "SR1|SR=1" "CYCB49|CYCB=49" "CYCB50|CYCB=50" "SR1CYCB49|SR=1 CYCB=49"; do
  tag="${spec%%|*}"; env="${spec##*|}"
  fail=0; first=""
  for f in $ALL; do
    [ -f "$f" ] || { echo "SKIP $f"; continue; }
    if [ -n "$env" ]; then eval "$env ./bin/ref_base < \"$f\" > /tmp/o_v 2>/dev/null"; else ./bin/ref_base < "$f" > /tmp/o_v 2>/dev/null; fi
    ./bin/ref_base < "$f" > /tmp/o_base 2>/dev/null
    if ! cmp -s /tmp/o_base /tmp/o_v; then fail=$((fail+1)); [ -z "$first" ] && first="$f"; fi
  done
  printf "%-12s bytecmp_fail=%s%s\n" "$tag" "$fail" "${first:+ first=$first}"
done
# ---- callgrind Ir: monster 巨例, BASE/CYCB49/CYCB50 并发(限3) ----
pids=()
for spec in "BASE|" "SR1|SR=1" "CYCB49|CYCB=49" "CYCB50|CYCB=50"; do
  tag="${spec%%|*}"; env="${spec##*|}"
  ( if [ -n "$env" ]; then eval "valgrind --tool=callgrind --callgrind-out-file=/tmp/cg3_${tag}_monster.txt --quiet $env ./bin/ref_base < monster.in >/dev/null 2>&1"; else valgrind --tool=callgrind --callgrind-out-file=/tmp/cg3_${tag}_monster.txt --quiet ./bin/ref_base < monster.in >/dev/null 2>&1; fi; \
    ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg3_${tag}_monster.txt); \
    printf "%-12s Ir(monster)=%s\n" "$tag" "$ir" ) >> sweep3_out.txt 2>&1 &
  pids+=($!)
done
for p in "${pids[@]}"; do wait $p; done
echo "===== DONE ====="
