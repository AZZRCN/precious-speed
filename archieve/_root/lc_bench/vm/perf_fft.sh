#!/bin/bash
# perf 剖析两堵 FFT 墙的真实周期热点
cd ~/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
g++ -O2 -g -std=c++23 -march=x86-64-v3 -o bin/div_D11_g src/div_D11.cpp 2>&1 | head -5
for c in a_max_b_random_02 length_ratio_integer_02; do
    echo "########## $c ##########"
    perf record -q -F 5000 --call-graph=no -o /tmp/pf_$c.data bin/div_D11_g < $IN/$c.in > /dev/null 2>/dev/null
    perf report -i /tmp/pf_$c.data --stdio --no-children -q --percent-limit 1.0 2>/dev/null | head -16
    echo "--- 事件计数 ---"
    perf stat -e cycles,instructions,cache-misses,branch-misses,L1-dcache-load-misses \
        -x, bin/div_D11_g < $IN/$c.in > /dev/null 2>/tmp/ps_$c.txt
    cat /tmp/ps_$c.txt
done
