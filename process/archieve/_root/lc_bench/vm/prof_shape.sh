#!/bin/bash
# 拿两堵 FFT 墙的真实 in / cyclic_m / 块数 / FFT 尺寸
cd ~/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
g++ -O2 -std=c++23 -march=x86-64-v3 -DPROFILE_DIV -o bin/div_D11_prof src/div_D11.cpp 2>&1 | head -5
for c in length_ratio_integer_02 a_max_b_random_02; do
    echo "########## $c ##########"
    rm -f prof_detail.log
    bin/div_D11_prof < $IN/$c.in > /dev/null 2>/dev/null
    echo "--- 前 26 行原始 ---"
    head -26 prof_detail.log
    echo "--- 分类汇总 (总ms / 次数 / 模板) ---"
    awk '{
        line=$0
        sub(/.*\[prof\][ ]*/,"",line)
        n=split(line,a," ")
        v=a[n-1]+0
        key=line
        gsub(/[0-9]+\.[0-9]+/,"#",key)
        gsub(/[0-9]+/,"N",key)
        cnt[key]++; tot[key]+=v
    } END{ for(k in cnt) printf "%9.3f ms  x%-4d  %s\n", tot[k], cnt[k], k }' prof_detail.log | sort -rn | head -14
done
