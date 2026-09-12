#!/bin/bash
# sweep.sh: 用 -D 编译 div.cpp 不同参数, 对瓶颈组跑 callgrind 取指令数, 输出对比表.
# 用法:
#   sweep.sh LEAF  8 9 10 11 12 13 14
#   sweep.sh MULBF 32 48 64 80 96
#   sweep.sh BZCUT 48 64 80 96 128
#   sweep.sh BNM   256 384 512 768
#   sweep.sh KDQ   32 48 64 96
#   sweep.sh RAW   "-DFOO=1" "-DBAR=2"      # 直接给完整 FLAGS
set -u
cd /home/azzr/precious_speed
GRP="length_ratio_0 length_ratio_1 length_ratio_2 length_ratio_3 length_ratio_4 length_ratio_5 amax_0 amax_1 large_0 large_1 medium_0 medium_1 small_0"
MODE="${1:-LEAF}"; shift
echo "mode=$MODE values=$*"
printf "%-22s %14s\n" "FLAGS" "TOTAL_Ir"
BEST=""; BESTV=999999999999999
for F in "$@"; do
  case "$MODE" in
    LEAF)  FLAGS="-DFFT_LEAF_LOG=$F" ;;
    MULBF) FLAGS="-DMULBF_MAX=$F" ;;
    BZCUT) FLAGS="-DBZ_CUTOFF=$F" ;;
    BNM)   FLAGS="-DBARRETT_NMIN=$F" ;;
    KDQ)   FLAGS="-DKD_QMAX=$F" ;;
    RAW)   FLAGS="$F" ;;
    *)     FLAGS="$F" ;;
  esac
  BIN="/tmp/sw_$$.bin"
  if ! g++ -O3 -std=c++17 -march=znver3 -mtune=znver3 $FLAGS -o "$BIN" hex_best/div.cpp 2>/tmp/ce.txt; then
    echo "$FLAGS COMPILE_FAIL"; head -3 /tmp/ce.txt; continue
  fi
  TOT=0
  for g in $GRP; do
    valgrind --tool=callgrind --cache-sim=no --branch-sim=no "$BIN" < cases_hex/$g.in >/dev/null 2>/tmp/vg.txt
    IR=$(grep 'I   refs' /tmp/vg.txt | tr -d ',' | awk '{print $NF}')
    TOT=$((TOT + ${IR:-0}))
  done
  printf "%-22s %14s\n" "$FLAGS" "$TOT"
  if [ "$TOT" -lt "$BESTV" ]; then BESTV=$TOT; BEST="$FLAGS"; fi
  rm -f "$BIN"
done
echo "BEST=$BEST ($BESTV)"
