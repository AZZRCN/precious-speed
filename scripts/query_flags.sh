#!/bin/bash
# 查询 Makefile 中 flags 相关的定义
MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
echo "=== 所有含 -g -O2 的变量定义 ==="
grep -nE "^[A-Z_]+ *=.*-g -O2" "$MAKEFILE" | head -20
echo ""
echo "=== CFLAGS/CXXFLAGS/LDFLAGS 相关定义 ==="
grep -nE "^(CFLAGS|CXXFLAGS|LDFLAGS|BOOT_CFLAGS|STAGE[0-9]_CFLAGS|INTERNAL_CFLAGS|GCC_CFLAGS|XLDFLAGS|T_CFLAGS|T_CXXFLAGS) " "$MAKEFILE" | head -30
echo ""
echo "=== gimple-to-cpp.o 的编译规则 ==="
grep -A2 "gimple-to-cpp.o:" "$MAKEFILE" | head -10
