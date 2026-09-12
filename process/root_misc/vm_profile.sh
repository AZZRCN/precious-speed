#!/bin/bash
B=$1; F=$2
echo "=== $B on $(basename $F) ==="
perf stat -r 2 -e cycles:u,instructions:u,cache-misses:u,cache-references:u,branch-misses:u,branches:u,L1-dcache-load-misses:u,LLC-load-misses:u -o /tmp/_pr.txt "$B" < "$F" >/dev/null 2>&1
awk '/cycles:u/{c=$1} /instructions:u/{i=$1} /cache-misses:u/{cm=$1} /cache-references:u/{cr=$1} /branch-misses:u/{bm=$1} /branches:u/{br=$1} /L1-dcache-load-misses:u/{l1=$1} /LLC-load-misses:u/{llc=$1} END{
  gsub(/,/,"",c); gsub(/,/,"",i); gsub(/,/,"",cm); gsub(/,/,"",cr); gsub(/,/,"",bm); gsub(/,/,"",br); gsub(/,/,"",l1); gsub(/,/,"",llc);
  printf "cycles=%s instr=%s IPC=%.3f\n", c, i, i/c;
  printf "cache-miss=%s cache-ref=%s  miss%%=%.2f\n", cm, cr, 100*cm/cr;
  printf "branch-miss=%s branches=%s  miss%%=%.3f\n", bm, br, 100*bm/br;
  printf "L1d-miss=%s  LLC-miss=%s\n", l1, llc;
}' /tmp/_pr.txt
