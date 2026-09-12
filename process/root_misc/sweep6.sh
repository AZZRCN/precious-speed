#!/bin/bash
# sweep6.sh: CYCB=50 接线前最后闸门
#   - bytecmp: CYCB=50 全 31 例 vs 默认(48), 确认数学等价(安全)
#   - callgrind Ir: max_r8 长商, CYCB 48/49/50, 确认单调降
cd /tmp/hexcmp
export OMP_NUM_THREADS=1
exec > sweep6_out.txt 2>&1
ALL=$(ls cases/case_*.in 2>/dev/null)
ALL="$ALL max_eq.in max_r2.in max_r8.in monster.in"
# bytecmp CYCB=50 vs BASE
fail=0; first=""
for f in $ALL; do [ -f "$f" ] || continue; env CYCB=50 ./bin/ref_base < "$f" > /tmp/o_v 2>/dev/null; ./bin/ref_base < "$f" > /tmp/o_base 2>/dev/null; if ! cmp -s /tmp/o_base /tmp/o_v; then fail=$((fail+1)); [ -z "$first" ] && first="$f"; fi; done
printf "CYCB50 bytecmp_fail=%s%s\n" "$fail" "${first:+ first=$first}"
# callgrind Ir: max_r8 长商, CYCB 48/49/50
for cb in 48 49 50; do
  env CYCB=$cb valgrind --tool=callgrind --callgrind-out-file=/tmp/cg6_r8_$cb.txt --quiet ./bin/ref_base < max_r8.in >/dev/null 2>&1
  ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg6_r8_$cb.txt)
  printf "CYCB=%s Ir(max_r8)=%s\n" "$cb" "$ir"
done
echo "===== DONE ====="
