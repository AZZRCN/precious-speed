#!/bin/bash
# callgrind Ir: baseline ref_zh (two-level) vs v2rec (single-level twfull); 全用例求和
DIR=~/precious_speed/cases_hex
measure() {
  local bin=$1 f=$2
  valgrind --tool=callgrind --callgrind-out-file=/tmp/_cg.txt --quiet "$bin" < "$f" >/dev/null 2>&1
  awk "/^totals:/{print \$2; exit}" /tmp/_cg.txt; rm -f /tmp/_cg.txt
}
echo "case | base(ref_zh) | v2rec | d%"
SA=0; SB=0
for f in "$DIR"/*.in; do
  b=$(basename "$f" .in)
  [ "$b" = "rnear_0" ] && continue   # 预崩溃用例(参考亦崩), 双边不计
  A=$(measure /tmp/hexcmp/bin/ref_zh "$f")
  B=$(measure /tmp/v2rec "$f")
  if [ -n "$A" ] && [ -n "$B" ]; then
    D=$(awk -v a="$A" -v b="$B" "BEGIN{printf \"%.3f\",(b-a)*100/a}")
    printf "%-18s %s %s %s%%\n" "$b" "$A" "$B" "$D"
    SA=$((SA+A)); SB=$((SB+B))
  else
    echo "$b INCOMPLETE A=$A B=$B"
  fi
done
echo "----- SUM base=$SA v2rec=$SB -----"
echo "OVERALL d% = $(awk -v a="$SA" -v b="$SB" "BEGIN{printf \"%.3f\",(b-a)*100/a}")"
