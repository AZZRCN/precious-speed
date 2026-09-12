cd /home/azzr/divbench
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file=/tmp/dbg.out ./v9_local_leaf10 < /home/azzr/hexbench/data/div/max_00.in >/tmp/dbg.stdout 2>/tmp/dbg.valerr
echo "RC_valgrind=$?"
echo "--- valgrind stderr (tail) ---"
tail -8 /tmp/dbg.valerr
echo "--- dbg.out exists? ---"
ls -la /tmp/dbg.out 2>/dev/null
echo "--- cg_annotate raw (first 14 lines) ---"
cg_annotate --auto=no --show=Irefs /tmp/dbg.out 2>&1 | head -14
echo "CG_DBG_DONE"
