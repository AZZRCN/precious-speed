#!/bin/bash
# UBSan 编译 - 通过 make 命令行传 flags
set -e

MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
cd /home/azzr/gcc-work/build

# 1. 恢复原始 Makefile
if [ -f "$MAKEFILE.bak.ubsan" ]; then
  cp "$MAKEFILE.bak.ubsan" "$MAKEFILE"
  echo "=== Makefile 已恢复 ==="
fi

# 2. 清理所有 .o 文件
echo "=== 清理 .o 文件 ==="
cd /home/azzr/gcc-work/build/gcc
rm -f *.o */*.o 2>/dev/null || true
cd /home/azzr/gcc-work/build
echo "=== 清理完成 ==="

# 3. 用 make 命令行传 flags 编译
# CFLAGS/CXXFLAGS 包含 -g -O2（保持原有优化）+ ubsan
# LDFLAGS 加 ubsan（链接 libubsan）
nohup make -j$(nproc) all-gcc \
  CFLAGS="-g -O2 -fsanitize=undefined" \
  CXXFLAGS="-g -O2 -fsanitize=undefined" \
  LDFLAGS="-static-libstdc++ -static-libgcc -fsanitize=undefined" \
  > make_ubsan.log 2>&1 &
echo $! > make_ubsan.pid
echo "=== UBSan make PID: $(cat make_ubsan.pid) ==="
sleep 12
echo "=== tail make_ubsan.log ==="
tail -8 make_ubsan.log
echo "=== error count ==="
grep -ci "error:" make_ubsan.log || echo 0
echo "=== 检查 ubsan 是否进入编译命令 ==="
grep -m3 "fsanitize" make_ubsan.log || echo "未找到 fsanitize"
