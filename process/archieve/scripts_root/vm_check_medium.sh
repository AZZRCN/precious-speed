#!/bin/bash
cd ~/div_bench
echo "Case count:"
head -1 div_medium_0.in
echo ""
echo "First 5 cases (len_a, len_b):"
tail -n +2 div_medium_0.in | head -5 | awk '{print length($1), length($2)}'
echo ""
echo "Length distribution (top 15):"
tail -n +2 div_medium_0.in | awk '{print length($1), length($2)}' | sort -n | uniq -c | sort -rn | head -15
echo ""
echo "Total digits sum (a + b):"
tail -n +2 div_medium_0.in | awk '{s += length($1) + length($2)} END {print s}'
