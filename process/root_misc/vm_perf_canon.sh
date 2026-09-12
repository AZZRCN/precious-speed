#!/bin/bash
BIN_CANON=/tmp/canon
BIN_REF=/tmp/refA
DIR=/tmp/lccases
meas() { perf stat -r 3 -e instructions:u -o /tmp/_pf.txt "$1" < "$2" >/dev/null 2>&1; awk '/instructions:u/{gsub(/,/,"",$1); print $1; exit}' /tmp/_pf.txt; }
echo "case | canon | refA | delta%(canon vs refA)"
mx_c=0 mx_r=0 mxc=""; sc=0 sr=0
for f in "$DIR"/*.in; do
  nm=$(basename "$f" .in)
  c=$(meas "$BIN_CANON" "$f"); r=$(meas "$BIN_REF" "$f")
  d=$(awk -v c="$c" -v r="$r" 'BEGIN{printf "%.3f",(r-c)/c*100}')
  printf "%s | %s | %s | %s%%\n" "$nm" "$c" "$r" "$d"
  sc=$((sc+c)); sr=$((sr+r))
  big=$(awk -v a="$c" -v b="$mx_c" 'BEGIN{print (a>b)?1:0}'); if [ "$big" = "1" ]; then mx_c=$c; mxc=$nm; fi
  big=$(awk -v a="$r" -v b="$mx_r" 'BEGIN{print (a>b)?1:0}'); if [ "$big" = "1" ]; then mx_r=$r; fi
done
echo "-----"
echo "SUM  canon=$sc refA=$sr"
echo "MAX  canon=$mx_c (@$mxc)  refA=$mx_r"
md=$(awk -v c="$mx_c" -v r="$mx_r" 'BEGIN{printf "%.3f",(r-c)/c*100}')
echo "MAX-point delta% (canon vs refA) = $md"
