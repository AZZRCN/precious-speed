#!/bin/bash
# usage: vm_pick.sh <bin1> <bin2> <case>
B1=$1; B2=$2; F=$3
get() {
  perf stat -r 3 -e instructions:u -o /tmp/_pf.txt "$1" < "$2" >/dev/null 2>&1
  awk '/instructions:u/{gsub(/,/,"",$1); print $1; exit}' /tmp/_pf.txt
}
a=$(get "$B1" "$F"); b=$(get "$B2" "$F")
echo "$B1=$a  $B2=$b  delta%($(awk -v x=$a -v y=$b 'BEGIN{printf "%.3f",(y-x)/x*100}')%)"
