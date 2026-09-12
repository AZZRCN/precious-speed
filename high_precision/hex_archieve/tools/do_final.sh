#!/bin/bash
# HEX div 标准验证 + Zen3 性能对比（固化基建，避免 Python 拼命令转义坑）
# 用法: bash do_final.sh
cd /home/azzr/divbench

# ---- 1. 编译候选 ----
sed "s/#define FFT_LEAF_LOG 11/#define FFT_LEAF_LOG 10/" v9.cpp > v9_leaf10.cpp
g++ -O2 -march=x86-64-v3 -std=c++23 -o v9_leaf10 v9_leaf10.cpp 2>&1 | tail -1; echo "RC_leaf=$?"
g++ -O2 -march=x86-64-v3 -std=c++23 -o hb_div /home/azzr/hexbench/div/div.cpp 2>&1 | tail -1; echo "RC_hb=$?"

# ---- 2. 27 个官方 HEX div 用例（int(a,16) 精确 oracle，大小写不敏感）----
for bin in v9_leaf10 hb_div; do
  ok=0; bad=0
  for f in /home/azzr/hexbench/data/div/*.in; do
    e=${f%.in}.exp
    if python3 hexcheck.py run ./$bin "$f" "$e" >/dev/null 2>&1; then ok=$((ok+1)); else bad=$((bad+1)); fi
  done
  echo "OFFICIAL $bin: OK=$ok BAD=$bad"
done

# ---- 3. Zen3 几何 (LL=32M/16) cachegrind I refs / LLd misses 对比 ----
VG="valgrind --tool=cachegrind --cache-sim=yes --branch-sim=yes --I1=32768,8,64 --D1=32768,8,64 --LL=33554432,16,64"
for bin in v9_leaf10 hb_div; do
  for sz in max mid large; do
    if [ "$sz" = "large" ]; then inf=/home/azzr/hexbench/data/div/large_00.in; else inf=bench_$sz.in; fi
    $VG --cachegrind-out-file=cg_${bin}_${sz}.out ./$bin < $inf >/dev/null 2>cg_${bin}_${sz}.log || true
    Ir=$(grep "I refs" cg_${bin}_${sz}.log | tr -s ' ')
    LL=$(grep "LLd misses" cg_${bin}_${sz}.log | tr -s ' ')
    echo "$bin $sz :: $Ir | $LL"
  done
done
