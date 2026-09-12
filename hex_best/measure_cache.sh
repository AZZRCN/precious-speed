#!/bin/bash
set -e
cd ~
FLAGS="-O3 -std=c++20 -march=znver3 -mtune=znver3 -w"
echo "[build] g++-15 $FLAGS"
g++-15 $FLAGS -o div_canon_now.bin div_canon_now.cpp 2>build.log || { echo "BUILD_FAIL"; tail -20 build.log; exit 1; }
echo "[build] ok"
printf 'case\tinstr\tl1loads\tl1miss\tcachemiss\tcacheref\n' > hexdiv_cache.tsv
for f in /tmp/lccases/*.in; do
  nm=$(basename "$f" .in)
  raw=$(perf stat -x, -e instructions:u,L1-dcache-loads,L1-dcache-load-misses,cache-misses,cache-references ./div_canon_now.bin < "$f" 2>&1 1>/dev/null)
  instr=$(echo "$raw" | awk -F, '$3=="instructions:u"{gsub(/,/,"",$1); print $1}')
  l1l=$(echo   "$raw" | awk -F, '$3=="L1-dcache-loads"{gsub(/,/,"",$1); print $1}')
  l1m=$(echo   "$raw" | awk -F, '$3=="L1-dcache-load-misses"{gsub(/,/,"",$1); print $1}')
  cm=$(echo    "$raw" | awk -F, '$3=="cache-misses"{gsub(/,/,"",$1); print $1}')
  cr=$(echo    "$raw" | awk -F, '$3=="cache-references"{gsub(/,/,"",$1); print $1}')
  instr=${instr:-0}; l1l=${l1l:-0}; l1m=${l1m:-0}; cm=${cm:-0}; cr=${cr:-0}
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$nm" "$instr" "$l1l" "$l1m" "$cm" "$cr" >> hexdiv_cache.tsv
done
echo "[done]"
cat hexdiv_cache.tsv
