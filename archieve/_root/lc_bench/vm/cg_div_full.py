import sys, re
sys.path.insert(0, r'D:\precious_speed\lc_bench\vm')
from vmctl import run

cmd = (
    "cd ~/divbench\n"
    "tot_base=0; tot_d1=0\n"
    "for f in $(ls ~/lcp/big_integer/division_of_big_integers/in/*.in | sort); do\n"
    "  c=$(basename $f .in)\n"
    "  valgrind --tool=callgrind --cache-sim=no --branch-sim=no --collect-jumps=no "
    "--callgrind-out-file=/tmp/cg_base.out ~/divbench/bin/div_D0_base < $f >/dev/null 2>/dev/null\n"
    "  b=$(callgrind_annotate --auto=yes /tmp/cg_base.out 2>/dev/null | grep 'PROGRAM TOTALS' | grep -oE '[0-9,]+' | head -1 | tr -d ',')\n"
    "  valgrind --tool=callgrind --cache-sim=no --branch-sim=no --collect-jumps=no "
    "--callgrind-out-file=/tmp/cg_d1.out ~/divbench/bin/div_D1_fused < $f >/dev/null 2>/dev/null\n"
    "  d=$(callgrind_annotate --auto=yes /tmp/cg_d1.out 2>/dev/null | grep 'PROGRAM TOTALS' | grep -oE '[0-9,]+' | head -1 | tr -d ',')\n"
    "  tot_base=$((tot_base + b)); tot_d1=$((tot_d1 + d))\n"
    "  printf '%s base=%s d1=%s\\n' \"$c\" \"$b\" \"$d\"\n"
    "done\n"
    "echo \"AGG base=$tot_base d1=$tot_d1\"\n"
)
run(cmd, timeout=900)
