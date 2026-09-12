#!/bin/bash
# 仅 GATE2 cache-sim + L3 宏扫描 (用小输入让 callgrind 在时限内跑完; 比值与尺寸无关)
set -e
cd /tmp/l3bench
# 1 查询 60000 limb (够用 FB/GB/FMG 多 MB, 压 L3), 单查询避免超 INCAP
python3 - <<'PY'
import random
rnd=random.Random(7)
def rh(n): return format(rnd.randrange(1,1<<(64*n)),'x')
with open('heavy_cg.txt','w') as f:
    f.write("1\n"); f.write(f"{rh(60000)} {rh(60000)}\n")
print("heavy_cg.txt written")
PY
echo "=== baseline v27 ==="
valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_opt.out  ./opt    heavy_cg.txt >/dev/null 2>&1
python3 l3_cgparse.py cg_opt.out
echo "=== L3 variant (32MB default) ==="
valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_L3.out  ./optL3  heavy_cg.txt >/dev/null 2>&1
python3 l3_cgparse.py cg_L3.out
echo "=== L3 macro sweep (wall 5r median + cache-sim) ==="
for MB in 8 16 24 32; do
  g++ -DL3_BYTES=$((MB*1024*1024)) -O3 -std=c++20 -march=znver3 -mtune=znver3 -w 393027_opt_L3.cpp -o optL3_$MB 2>&1 | head
  w=()
  for r in 1 2 3 4 5; do
    s=$(date +%s%N); taskset -c 2 ./optL3_$MB heavy.txt >/dev/null; e=$(date +%s%N)
    w+=($(( (e-s)/1000000 )))
  done
  IFS=$'\n' ws=($(sort -n <<<"${w[*]}")); unset IFS
  valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_${MB}.out ./optL3_$MB heavy_cg.txt >/dev/null 2>&1
  echo "--- L3_BYTES=${MB}MB  wall_med=${ws[2]}ms ---"
  python3 l3_cgparse.py cg_${MB}.out
done
echo "DONE"
