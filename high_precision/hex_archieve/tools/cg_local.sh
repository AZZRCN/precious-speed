cd /home/azzr/divbench
echo "--- cachegrind (LOCAL v9 LEAF=10) via valgrind summary ---"
for cf in max_00 large_00 a_max_b_random_02 length_ratio_integer_02 burnikel_ziegler_bound_03; do
  fin=/home/azzr/hexbench/data/div/${cf}.in
  valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=/tmp/cgl_${cf}.out ./v9_local_leaf10 < $fin >/dev/null 2>/tmp/cgl_${cf}.err
  Irefs=$(awk '/I refs:/{print $4; exit}' /tmp/cgl_${cf}.err)
  LLd=$(awk '/LLd misses:/{print $4; exit}' /tmp/cgl_${cf}.err)
  echo "local_v9 $cf :: Irefs=$Irefs LLd=$LLd"
done
echo "CG_LOCAL_DONE"
