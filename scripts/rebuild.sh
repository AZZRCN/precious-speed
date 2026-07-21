#!/bin/bash
# 重新编译 gcc 主体（仅 gimple-to-cpp.c 变化）
cd /home/azzr/gcc-work/build
nohup make -j$(nproc) all-gcc > make_patch2.log 2>&1 &
echo $! > make_patch2.pid
echo "=== make PID: $(cat make_patch2.pid) ==="
sleep 8
echo "=== tail make_patch2.log ==="
tail -15 make_patch2.log
echo "=== error count ==="
grep -ci "error:" make_patch2.log
