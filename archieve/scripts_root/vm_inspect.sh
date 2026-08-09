#!/bin/bash
cd ~/div_bench
echo "Case count:"
head -1 div_medium_0.in
echo ""
echo "Length distribution (len_a, len_b):"
awk 'NR>1{print length($1), length($2)}' div_medium_0.in | sort | uniq -c | sort -rn | head -15
echo ""
echo "Total lines:"
wc -l div_medium_0.in
