#!/bin/bash
# 查找编译命令模板和 -O2 来源
MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
echo "=== 查找 -O2 来源 ==="
grep -nE "(-O2|OPTIMIZE|BOOT_CFLAGS|GCC_CFLAGS)" "$MAKEFILE" | head -20
echo ""
echo "=== 查找 .cc.o 或 %.o 编译规则 ==="
grep -nE "(\.cc\.o|\.c\.o|%.o:|compile.*\.cc)" "$MAKEFILE" | head -15
echo ""
echo "=== 查找 g++ 编译命令模板（含 -DIN_GCC）==="
grep -nE "g\+\+.*-DIN_GCC|CXX.*-DIN_GCC" "$MAKEFILE" | head -5
echo ""
echo "=== 查看 INTERNAL_CFLAGS 完整定义 ==="
grep -nE "^INTERNAL_CFLAGS|^GCC_CFLAGS|^WARN_CFLAGS|^LOOSE_WARN|^STRICT_WARN" "$MAKEFILE" | head -15
