#!/bin/bash
# 量化 newton 路径(大 FFT)的 cache 行为: cache-misses / L1-dcache-misses / LLC-misses / instructions:u
BIN=/tmp/optB
CASE=/tmp/lccases/length_ratio_integer_0.in
if [ ! -x "$BIN" ] || [ ! -f "$CASE" ]; then echo "MISSING $BIN or $CASE"; exit 1; fi
echo "### cache+instr for newton-path case (length_ratio_integer_0) ###"
perf stat -r 3 -e instructions:u,cache-misses,L1-dcache-misses,LLC-misses,cache-references,L1-dcache-loads,LLC-loads -o /tmp/_cache.txt "$BIN" < "$CASE" >/dev/null 2>&1
grep -E "instructions:u|cache-misses|L1-dcache-misses|LLC-misses|cache-references|L1-dcache-loads|LLC-loads" /tmp/_cache.txt | sed 's/^ *//'
echo "### input peek (a,b hex lengths -> na,nb) ###"
head -c 200 "$CASE" | tr ' ' '\n' | head -4
