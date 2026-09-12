#!/bin/bash
# 双二进制 callgrind Ir 对比: ref_base vs ref_flat, 逐用例报 delta%
A=/tmp/hexcmp/bin/ref_base
B=/tmp/hexcmp/bin/ref_flat
export OMP_NUM_THREADS=1
ls cases/case_*.in max_eq.in max_r2.in max_r8.in monster.in | xargs -P 14 -I{} bash -c '
  f="{}"; b=$(basename "$f" .in)
  IA=$(valgrind --tool=callgrind --callgrind-out-file=/tmp/cgA_$$.txt --quiet '"$A"' < "$f" >/dev/null 2>&1; awk "/^totals:/{print \$2; exit}" /tmp/cgA_$$.txt; rm -f /tmp/cgA_$$.txt)
  IB=$(valgrind --tool=callgrind --callgrind-out-file=/tmp/cgB_$$.txt --quiet '"$B"' < "$f" >/dev/null 2>&1; awk "/^totals:/{print \$2; exit}" /tmp/cgB_$$.txt; rm -f /tmp/cgB_$$.txt)
  if [ -n "$IA" ] && [ -n "$IB" ] && [ "$IA" != "0" ]; then
    D=$(echo "scale=3; ($IB-$IA)*100/$IA" | bc)
    echo "$b base=$IA flat=$IB d%=$D"
  fi
' | sort -k1
