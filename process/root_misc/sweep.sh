#!/bin/bash
# 扫描 cyclic 精度预算 CYCB=X vs 基准 47: 全用例字节比对 (DIFF/CRASH = 不安全)
X=$1
LOG=/tmp/sweep_${X}.log
: > "$LOG"
export OMP_NUM_THREADS=1
ls cases/case_*.in | xargs -P 14 -I{} bash -c '
  f="{}"
  b=$(basename "$f")
  CYCB=47 /tmp/hexcmp/bin/ref_zhbase < "$f" > /tmp/o47_$$_$b 2>/dev/null; rc47=$?
  CYCB='"$X"' /tmp/hexcmp/bin/ref_zhbase < "$f" > /tmp/oX_$$_$b 2>/dev/null; rcX=$?
  if [ $rc47 -ne 0 ] || [ $rcX -ne 0 ]; then echo "CRASH $b 47=$rc47 X=$rcX"; rm -f /tmp/o47_$$_$b /tmp/oX_$$_$b; exit 0; fi
  if ! cmp -s /tmp/o47_$$_$b /tmp/oX_$$_$b; then echo "DIFF $b"; fi
  rm -f /tmp/o47_$$_$b /tmp/oX_$$_$b
' >> "$LOG" 2>&1
echo "SWEEP X=$X fails=$(grep -c -E 'CRASH|DIFF' "$LOG") total=$(ls cases/case_*.in | wc -l)"
grep -E 'CRASH|DIFF' "$LOG" || echo "NONE"
