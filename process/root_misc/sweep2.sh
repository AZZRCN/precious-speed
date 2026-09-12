#!/bin/bash
# sweep2.sh: 修 bug + 加速版 GLM 假设实测
#   - bytecmp: 通配 cases/* 避免硬编码错位; 各变体 vs 默认 ref_base
#   - callgrind Ir: 仅巨例 monster, 对 BASE/CYCB49/CYCB50 并发 (内存安全)
cd /tmp/hexcmp
export OMP_NUM_THREADS=1
exec > sweep2_out.txt 2>&1
ALL=$(ls cases/case_*.in 2>/dev/null)
ALL="$ALL max_eq.in max_r2.in max_r8.in monster.in"
echo "CASE_COUNT=$(echo $ALL | wc -w)"
# ---- bytecmp 全变体 vs 默认 ref_base ----
for env in "" "SR=1" "CYCB=49" "CYCB=50" "SR=1 CYCB=49"; do
  fail=0; first=""
  for f in $ALL; do
    [ -f "$f" ] || { echo "SKIP $f (missing)"; continue; }
    eval "$env ./bin/ref_base < \"$f\" > /tmp/o_v 2>/dev/null"
    ./bin/ref_base < "$f" > /tmp/o_base 2>/dev/null
    if ! cmp -s /tmp/o_base /tmp/o_v; then fail=$((fail+1)); [ -z "$first" ] && first="$f"; fi
  done
  tag="${env:-BASE}"
  printf "%-12s bytecmp_fail=%s%s\n" "$tag" "$fail" "${first:+ first=$first}"
done
# ---- callgrind Ir: monster 巨例, BASE/CYCB49/CYCB50 并发(限3) ----
pids=()
for env in "" "CYCB=49" "CYCB=50"; do
  tag="${env:-BASE}"
  ( eval "valgrind --tool=callgrind --callgrind-out-file=/tmp/cg2_${tag}_monster.txt --quiet $env ./bin/ref_base < monster.in >/dev/null 2>&1"; \
    ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg2_${tag}_monster.txt); \
    printf "%-12s Ir(monster)=%s\n" "$tag" "$ir" ) &
  pids+=($!)
done
for p in "${pids[@]}"; do wait $p; done
echo "===== DONE ====="
