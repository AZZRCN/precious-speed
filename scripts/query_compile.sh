#!/bin/bash
MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
echo "=== ALL_CXXFLAGS 定义（行 1040-1050）==="
sed -n '1040,1050p' "$MAKEFILE"
echo ""
echo "=== COMPILE 定义 ==="
grep -nE "^COMPILE|^COMPILE\.base" "$MAKEFILE" | head -5
echo ""
echo "=== 查找 -O2 的所有出现 ==="
grep -nE "(-O2|OPTIMIZE)" "$MAKEFILE" | head -15
echo ""
echo "=== CFLAGS-gimple-to-cpp.o 或 per-file flags ==="
grep -nE "CFLAGS-|CXXFLAGS-" "$MAKEFILE" | head -10
