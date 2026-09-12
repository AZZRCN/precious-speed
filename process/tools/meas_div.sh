cd /home/azzr/precious_speed
for b in div_zn div_molder_zn div_molder_nb_zn; do
  for c in div_large div_max div_length_ratio div_small div_medium; do
    taskset -c 0 perf stat -e instructions -x, ./hex_best/$b.bin < cases/$c.in >/dev/null 2>/tmp/p.txt
    v=$(grep -E '^[^,]+,,instructions' /tmp/p.txt | head -1 | cut -d, -f1)
    echo "$b $c $v"
  done
done
