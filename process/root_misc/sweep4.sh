#!/bin/bash
# sweep4.sh: 纯顺序 callgrind (避免并行竞态), 干净 tag, 带 rc 诊断
cd /tmp/hexcmp
export OMP_NUM_THREADS=1
exec > sweep4_out.txt 2>&1
for spec in "BASE|" "SR1|SR=1" "CYCB49|CYCB=49" "CYCB50|CYCB=50"; do
  tag="${spec%%|*}"; env="${spec##*|}"
  if [ -n "$env" ]; then eval "valgrind --tool=callgrind --callgrind-out-file=/tmp/cg4_${tag}_monster.txt --quiet $env ./bin/ref_base < monster.in >/dev/null 2>&1"; else valgrind --tool=callgrind --callgrind-out-file=/tmp/cg4_${tag}_monster.txt --quiet ./bin/ref_base < monster.in >/dev/null 2>&1; fi
  rc=$?
  ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg4_${tag}_monster.txt)
  printf "%-12s rc=%s Ir(monster)=%s\n" "$tag" "$rc" "$ir"
done
echo "===== DONE ====="
