#!/bin/bash
BIN_OPT=/tmp/opt2
BIN_REF=/tmp/ref
DIR=/tmp/lccases

meas() {
  local bin=$1 f=$2
  perf stat -r 3 -e instructions:u -o /tmp/_pf.txt "$bin" < "$f" >/dev/null 2>&1
  grep -oE '[0-9,]+ +instructions:u' /tmp/_pf.txt | grep -oE '[0-9,]+' | head -1 | tr -d ','
}

echo "case | opt | ref | delta%"
mx_o=0 mx_r=0 mxc=""
so=0 sr=0
for f in "$DIR"/*.in; do
  nm=$(basename "$f" .in)
  o=$(meas "$BIN_OPT" "$f")
  r=$(meas "$BIN_REF" "$f")
  d=$(awk -v o="$o" -v r="$r" 'BEGIN{printf "%.3f",(r-o)/o*100}')
  printf "%s | %s | %s | %s%%\n" "$nm" "$o" "$r" "$d"
  so=$((so+o)); sr=$((sr+r))
  big=$(awk -v a="$o" -v b="$mx_o" 'BEGIN{print (a>b)?1:0}')
  if [ "$big" = "1" ]; then mx_o=$o; mxc=$nm; fi
  big=$(awk -v a="$r" -v b="$mx_r" 'BEGIN{print (a>b)?1:0}')
  if [ "$big" = "1" ]; then mx_r=$r; fi
done
echo "-----"
echo "SUM  opt=$so ref=$sr"
echo "MAX  opt=$mx_o (@$mxc)  ref=$mx_r"
md=$(awk -v o="$mx_o" -v r="$mx_r" 'BEGIN{printf "%.3f",(r-o)/o*100}')
echo "MAX-point delta% = $md"
