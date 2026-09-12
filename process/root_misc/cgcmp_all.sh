#!/bin/bash
# 全用例 callgrind Ir: CYCB=$1 vs $2, 报告 delta%
A=$1; B=$2
export OMP_NUM_THREADS=1
ls cases/case_*.in | xargs -P 14 -I{} bash -c '
  f="{}"; b=$(basename "$f")
  IA=$(CYCB='"$A"' valgrind --tool=callgrind --callgrind-out-file=/tmp/cgA_$$.txt --quiet /tmp/hexcmp/bin/ref_zhbase < "$f" >/dev/null 2>&1; awk "/^totals:/{print \$2; exit}" /tmp/cgA_$$.txt; rm -f /tmp/cgA_$$.txt)
  IB=$(CYCB='"$B"' valgrind --tool=callgrind --callgrind-out-file=/tmp/cgB_$$.txt --quiet /tmp/hexcmp/bin/ref_zhbase < "$f" >/dev/null 2>&1; awk "/^totals:/{print \$2; exit}" /tmp/cgB_$$.txt; rm -f /tmp/cgB_$$.txt)
  if [ -n "$IA" ] && [ -n "$IB" ] && [ "$IA" != "0" ]; then
    D=$(echo "scale=3; ($IB-$IA)*100/$IA" | bc)
    echo "$b $A=$IA $B=$IB d%=$D"
  fi
' | sort -k1
