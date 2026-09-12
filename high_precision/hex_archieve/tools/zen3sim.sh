#!/usr/bin/env bash
# zen3sim.sh — 按 AMD EPYC 7B13 (Zen3) 缓存几何分析程序
# 用法: ./zen3sim.sh ./prog [input_file] [outdir_tag]
# 依据: D:\precious_speed\ZEN3_CACHE_SIM_ON_INTEL_VM.md
set -uo pipefail

PROG="${1:?usage: $0 ./prog [input] [tag]}"; IN="${2:-/dev/null}"
TAG="${3:-$(basename "$PROG").$(date +%H%M%S)}"
OUT="zen3sim.$TAG"; mkdir -p "$OUT"

# ── Zen3 / EPYC 7B13 缓存几何（硬编码，勿从 lscpu 读）──────────────
I1="32768,8,64"        # L1i  32 KiB  8-way
D1="32768,8,64"        # L1d  32 KiB  8-way
L2="524288,8,64"       # L2  512 KiB  8-way  (每核私有)
L3="33554432,16,64"    # L3   32 MiB 16-way  (单 CCX)
L3X="37748736,18,64"   # L3 exclusive 上界 36 MiB (18-way 规避 set-count 约束)

run() {  # $1=tag  $2=LL参数
  echo "-- [$1] LL=$2"
  setarch "$(uname -m)" -R valgrind --tool=cachegrind \
      --cache-sim=yes --branch-sim=yes \
      --I1="$I1" --D1="$D1" --LL="$2" \
      --cachegrind-out-file="$OUT/cg.$1.out" \
      "$PROG" < "$IN" > "$OUT/stdout.$1" 2> "$OUT/cg.$1.log"
  grep -E 'I   refs|I1  misses|D   refs|D1  misses|LLd misses|LL  misses|Mispredicts' \
      "$OUT/cg.$1.log" | sed 's/^==[0-9]*== /   /'
}

echo "=== Zen3 (EPYC 7B13) cache simulation : $PROG < $IN ==="
run l2  "$L2"
run l3  "$L3"
if [ "${SKIP_L3X:-0}" != "1" ]; then
  run l3x "$L3X" || echo "   (l3x skipped)"
fi
echo "OUT=$OUT"
