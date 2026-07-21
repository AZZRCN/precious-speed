#!/bin/bash
MAKEFILE=/home/azzr/gcc-work/build/gcc/Makefile
echo "=== .cc.o 规则（行 1130-1160）==="
sed -n '1130,1160p' "$MAKEFILE"
echo ""
echo "=== 查找 CXX 编译命令变量 ==="
grep -nE "^(CXX|CXXCOMPILE|CXXFLAGS|ALL_CXXFLAGS|COMPILE\.cc)" "$MAKEFILE" | head -15
