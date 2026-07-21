#!/bin/bash
# 测试 -fo3-pre-expand，收集 UBSan 运行时报告
set -e

CC1PLUS=/home/azzr/gcc-work/build/gcc/cc1plus
TEST_IN=/tmp/test_o3.cpp
TEST_OUT=/tmp/test_out.cpp
UBSAN_LOG=/tmp/ubsan_runtime.log

echo "=== cc1plus 版本 ==="
$CC1PLUS --version 2>&1 | head -3 || echo "version query failed"

echo ""
echo "=== 测试 -fo3-pre-expand ==="
echo "输入: $TEST_IN"
echo "输出: $TEST_OUT"
echo "UBSan 日志: $UBSAN_LOG"

# 设置 UBSan 选项：打印堆栈、不中止
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0:log_path=$UBSAN_LOG"

# 运行 -fo3-pre-expand
# cc1plus 直接调用，需要 -quiet 抑制banner，-O2 设置优化级别
# -fo3-pre-expand 是我们的自定义选项
echo ""
echo "=== 运行 cc1plus ==="
$CC1PLUS -quiet -O2 -fo3-pre-expand $TEST_IN -o $TEST_OUT 2>&1 || echo "EXIT CODE: $?"

echo ""
echo "=== UBSan 运行时报告 ==="
if [ -f "$UBSAN_LOG" ]; then
    echo "UBSan 报告行数: $(wc -l < $UBSAN_LOG)"
    echo "--- 前 50 行 ---"
    head -50 "$UBSAN_LOG"
else
    echo "无 UBSan 报告（$UBSAN_LOG 不存在）"
fi

echo ""
echo "=== 输出文件检查 ==="
if [ -f "$TEST_OUT" ]; then
    echo "输出文件大小: $(wc -c < $TEST_OUT) 字节"
    echo "输出文件行数: $(wc -l < $TEST_OUT) 行"
    echo "--- 前 30 行 ---"
    head -30 "$TEST_OUT"
else
    echo "输出文件 $TEST_OUT 不存在"
fi
