import sys
sys.path.insert(0, r'D:\precious_speed\lc_bench\vm')
from vmctl import run, put

FLAGS = "-O2 -std=c++23 -march=x86-64-v3"
SRC = r"D:\precious_speed\lc_bench\exe\div_D1_fused.cpp"

# 1) upload
put(SRC, "/home/azzr/divbench/src/div_D1_fused.cpp")

# 2) build + verify + callgrind compare (base vs D1)
cmd = (
    "cd ~/divbench\n"
    "g++ " + FLAGS + " -o bin/div_D1_fused src/div_D1_fused.cpp && echo BUILD_OK\n"
    "echo '===== VERIFY D1 ====='\n"
    "python3 ~/verify_official.py div ~/divbench/bin/div_D1_fused\n"
    "echo '===== CALLGRIND ====='\n"
    "for c in length_ratio_integer_00 burnikel_ziegler_bound_00; do\n"
    "  for b in div_D0_base div_D1_fused; do\n"
    "    valgrind --tool=callgrind --cache-sim=no --branch-sim=no --collect-jumps=no "
    "--callgrind-out-file=/tmp/cg_${b}_${c}.out ~/divbench/bin/${b} "
    "< ~/lcp/big_integer/division_of_big_integers/in/${c}.in >/dev/null 2>/dev/null\n"
    "    TOT=$(callgrind_annotate --auto=yes /tmp/cg_${b}_${c}.out 2>/dev/null | grep 'PROGRAM TOTALS')\n"
    "    ADC=$(callgrind_annotate --auto=yes /tmp/cg_${b}_${c}.out 2>/dev/null | grep 'absDivBasicCore' | head -1)\n"
    "    echo \"${b} ${c} | TOTAL=${TOT} | ADC=${ADC}\"\n"
    "  done\n"
    "done\n"
)
run(cmd, timeout=600)
