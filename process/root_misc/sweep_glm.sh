#!/bin/bash
# sweep_glm.sh: 纯 env 开关实测 GLM 假设 H1(CYCB)/H4(SR)
#   双闸门: bytecmp 全 31 例 vs 基线(SR=0,CYCB=48) + callgrind Ir(3 代表尺寸)
#   用法: bash sweep_glm.sh
cd /tmp/hexcmp
export OMP_NUM_THREADS=1
ALL="cases/case_001.in cases/case_002.in cases/case_003.in cases/case_004.in cases/case_005.in cases/case_006.in cases/case_007.in cases/case_008.in cases/case_009.in cases/case_010.in cases/case_011.in cases/case_012.in cases/case_013.in cases/case_014.in cases/case_015.in cases/case_016.in cases/case_017.in cases/case_018.in cases/case_019.in cases/case_020.in cases/case_021.in cases/case_022.in cases/case_023.in cases/case_024.in cases/case_025.in cases/case_026.in cases/case_027.in max_eq.in max_r2.in max_r8.in monster.in"
IRCASES="monster.in max_r8.in cases/case_024.in"

run_variant() {
  local label="$1"; local envprefix="$2"
  # ---- 正确性: 与基线 bytecmp ----
  local fail=0; local firstdiff=""
  for f in $ALL; do
    eval "$envprefix ./bin/ref_base < \"$f\" > /tmp/o_v 2>/dev/null"
    ./bin/ref_base < "$f" > /tmp/o_base 2>/dev/null
    if ! cmp -s /tmp/o_base /tmp/o_v; then fail=$((fail+1)); [ -z "$firstdiff" ] && firstdiff="$f"; fi
  done
  printf "%-12s bytecmp_fail=%s%s\n" "$label" "$fail" "${firstdiff:+ first=$firstdiff}"
  # ---- Ir ----
  for f in $IRCASES; do
    eval "valgrind --tool=callgrind --callgrind-out-file=/tmp/cg_$label.txt --quiet $envprefix ./bin/ref_base < \"$f\" >/dev/null 2>&1"
    local ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg_$label.txt)
    printf "%-12s Ir(%-16s) = %s\n" "$label" "$f" "$ir"
  done
}
echo "===== GLM sweep (bytecmp vs BASE + callgrind Ir) ====="
run_variant "BASE"       ""
run_variant "SR1"        "SR=1 "
run_variant "CYCB49"     "CYCB=49 "
run_variant "CYCB50"     "CYCB=50 "
run_variant "SR1CYCB49"  "SR=1 CYCB=49 "
echo "===== DONE ====="
