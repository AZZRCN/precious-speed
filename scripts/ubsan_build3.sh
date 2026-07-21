#!/bin/bash
# UBSan 编译 v3 - 去掉 -static-libstdc++ -static-libgcc，避免干扰 libubsan 链接
set -e

cd /home/azzr/gcc-work/build

# 1. 清理失败的 .o（只清理链接失败的 generator，保留已编译的 .o 节省时间）
echo "=== 清理 generator .o（强制重新链接）==="
cd /home/azzr/gcc-work/build/gcc
rm -f build/*.o *.o 2>/dev/null || true
cd /home/azzr/gcc-work/build
echo "=== 清理完成 ==="

# 2. 重新编译，LDFLAGS 去掉 -static-libstdc++ -static-libgcc
nohup make -j$(nproc) all-gcc \
  CFLAGS="-g -O2 -fsanitize=undefined" \
  CXXFLAGS="-g -O2 -fsanitize=undefined" \
  LDFLAGS="-fsanitize=undefined" \
  > make_ubsan3.log 2>&1 &
echo $! > make_ubsan3.pid
echo "=== UBSan v3 make PID: $(cat make_ubsan3.pid) ==="
sleep 15
echo "=== error count ==="
grep -ci "error:" make_ubsan3.log || echo 0
echo "=== undefined reference count ==="
grep -c "undefined reference" make_ubsan3.log || echo 0
echo "=== tail ==="
tail -6 make_ubsan3.log
