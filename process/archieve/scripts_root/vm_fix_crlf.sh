#!/bin/bash
cd ~/div_bench
for f in *.in; do
    tr -d '\r' < "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done
echo "Converted files:"
file *.in | head -3
echo "---"
head -c 20 add_max_0.in | xxd | head -1
