#!/bin/bash
# UBSan 全量编译脚本
# 修改 CFLAGS/CXXFLAGS/LDFLAGS 注入 -fsanitize=undefined，然后全量重新编译
set -e

MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
cd /home/azzr/gcc-work/build

# 1. 备份 Makefile
if [ ! -f "$MAKEFILE.bak.ubsan" ]; then
  cp "$MAKEFILE" "$MAKEFILE.bak.ubsan"
  echo "=== Makefile 已备份 ==="
else
  echo "=== 备份已存在，从备份恢复 Makefile ==="
  cp "$MAKEFILE.bak.ubsan" "$MAKEFILE"
fi

# 2. 修改 CFLAGS / CXXFLAGS / LDFLAGS
# 原值: CFLAGS = -g / CXXFLAGS = -g / LDFLAGS = -static-libstdc++ -static-libgcc
sed -i 's|^CFLAGS = -g$|CFLAGS = -g -fsanitize=undefined|' "$MAKEFILE"
sed -i 's|^CXXFLAGS = -g$|CXXFLAGS = -g -fsanitize=undefined|' "$MAKEFILE"
sed -i 's|^LDFLAGS = -static-libstdc++ -static-libgcc$|LDFLAGS = -static-libstdc++ -static-libgcc -fsanitize=undefined|' "$MAKEFILE"

echo "=== 修改后的 flags ==="
grep -nE "^(CFLAGS|CXXFLAGS|LDFLAGS) =" "$MAKEFILE" | head -5

# 3. 清理所有 .o 文件（强制全量重新编译）
echo "=== 清理所有 .o 文件 ==="
cd /home/azzr/gcc-work/build/gcc
rm -f *.o */*.o 2>/dev/null || true
echo "=== 清理完成 ==="

# 4. 全量重新编译
cd /home/azzr/gcc-work/build
nohup make -j$(nproc) all-gcc > make_ubsan.log 2>&1 &
echo $! > make_ubsan.pid
echo "=== UBSan make PID: $(cat make_ubsan.pid) ==="
sleep 10
echo "=== tail make_ubsan.log ==="
tail -15 make_ubsan.log
echo "=== error count (initial) ==="
grep -ci "error:" make_ubsan.log || echo 0
echo "=== 检查编译命令是否含 ubsan ==="
grep -m1 "fsanitize" make_ubsan.log || echo "尚未出现 fsanitize"
