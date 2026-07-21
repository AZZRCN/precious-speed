#!/bin/bash
# 解压 patch 文件 + 修改 Makefile + 重新编译 gcc 主体

# 1. 解压 patch 文件
cd /home/azzr/gcc-work/gcc-11.4.0
tar -xzf /tmp/patch_files.tar.gz
echo "=== 解压完成 ==="
ls -la gcc/gimple-to-cpp.c gcc/gimple-to-cpp.h

# 2. 在 build/gcc/Makefile 中添加 gimple-to-cpp.o
MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
if ! grep -q "gimple-to-cpp.o" "$MAKEFILE"; then
  # 只替换第一次出现（OBJS 列表中）
  sed -i '0,/gimple-pretty-print.o/{s|gimple-pretty-print.o|gimple-pretty-print.o \\\
\tgimple-to-cpp.o|}' "$MAKEFILE"
  echo "=== Makefile 已添加 gimple-to-cpp.o ==="
  grep -n "gimple-to-cpp.o" "$MAKEFILE" | head -3
else
  echo "=== Makefile 已包含 gimple-to-cpp.o ==="
fi

# 3. 重新编译 gcc 主体（跳过 libsanitizer 等运行时库）
cd /home/azzr/gcc-work/build
nohup make -j$(nproc) all-gcc > make_patch.log 2>&1 &
echo $! > make_patch.pid
echo "=== make PID: $(cat make_patch.pid) ==="
sleep 5
echo "=== tail make_patch.log ==="
tail -10 make_patch.log
