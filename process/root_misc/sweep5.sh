#!/bin/bash
# sweep5.sh: 修 eval env 位置 bug -> 用 env 命令显式设环境; 顺序 callgrind 巨例 monster
cd /tmp/hexcmp
export OMP_NUM_THREADS=1
exec > sweep5_out.txt 2>&1
for spec in "BASE|" "SR1|SR=1" "CYCB49|CYCB=49" "CYCB50|CYCB=50"; do
  tag="${spec%%|*}"; env="${spec##*|}"
  if [ -n "$env" ]; then
    env $env valgrind --tool=callgrind --callgrind-out-file=/tmp/cg5_${tag}_monster.txt --quiet ./bin/ref_base < monster.in >/dev/null 2>&1
  else
    valgrind --tool=callgrind --callgrind-out-file=/tmp/cg5_${tag}_monster.txt --quiet ./bin/ref_base < monster.in >/dev/null 2>&1
  fi
  rc=$?
  ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg5_${tag}_monster.txt)
  printf "%-12s rc=%s Ir(monster)=%s\n" "$tag" "$rc" "$ir"
done
echo "===== DONE ====="
