#!/bin/bash
# 算法主线测量: 对 path3/5 + zhB(命中#26) 的大例, 分别跑 base/zh 的 callgrind, 提取 Ir 对比。
# 每批 2 文件 x (base,zh) = 4 并发, 占 4 核防 OOM。实时打印 CG 行。
cd /tmp/hexcmp
run(){
  f=$1; b=$2
  valgrind --tool=callgrind --callgrind-out-file=/tmp/cg_${b}_${f}.txt --quiet ./bin/$b < cases_dir/$f.in > /dev/null 2>&1
  ir=$(awk '/^totals:/{print $2; exit}' /tmp/cg_${b}_${f}.txt)
  echo "CG $f $b Ir=$ir"
}
for f in case_0036 case_0007; do run $f ref_zhbase & run $f ref_zh & done; wait; echo "BATCH1_DONE"
for f in case_0050 case_0005; do run $f ref_zhbase & run $f ref_zh & done; wait; echo "BATCH2_DONE"
echo "ALL_CG_DONE"
