cd /home/azzr/divbench
echo === GATE L10 ===
ok=0; bad=0
for f in /home/azzr/hexbench/data/mul/*.in; do
  e="${f%.in}.exp"
  python3 mulcheck.py run ./L10 "$f" "$e" >/dev/null 2>&1 && ok=$((ok+1)) || bad=$((bad+1))
done
echo "GATE L10 OK=$ok BAD=$bad"
echo === WALL L10 max_max_06 best-of-3 ===
inf=/home/azzr/hexbench/data/mul/max_max_06.in; best=999999
for r in 1 2 3; do
  s=$(date +%s%N); ./L10 < $inf >/dev/null; e=$(date +%s%N); d=$(( (e-s)/1000000 ))
  [ $d -lt $best ] && best=$d
done
echo "WALL L10 max_max_06 best=${best}ms"
