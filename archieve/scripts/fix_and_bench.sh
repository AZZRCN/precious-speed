#!/bin/bash
# fix_and_bench.sh
# 转换 CRLF → LF 并重新基准测试
cd /tmp/bench2

echo "=== Check CRLF ==="
for f in test_add_1M_1M.txt test_mul_500k_500k.txt test_div_1M_500k.txt; do
    echo -n "$f: "
    if file "$f" | grep -q CRLF; then echo "CRLF"; else echo "LF"; fi
done

echo ""
echo "=== Convert all test_*.txt to LF ==="
for f in test_*.txt; do
    sed -i 's/\r//g' "$f"
done
echo "Done."

echo ""
echo "=== Verify DIV ==="
echo -n "moptm: "; ./moptm_DIV < test_div_1M_500k.txt | head -c 50; echo ""
echo -n "best:  "; ./best_DIV < test_div_1M_500k.txt | head -c 50; echo ""

echo ""
echo "=== Correctness (all) ==="
verify() {
    local name=$1 exe_m=$2 exe_b=$3 test=$4
    ./${exe_m} < ${test} > out_m.txt 2>/dev/null
    ./${exe_b} < ${test} > out_b.txt 2>/dev/null
    if diff -q out_m.txt out_b.txt > /dev/null 2>&1; then
        echo "  ${name}: PASS"
    else
        echo "  ${name}: FAIL"
        diff out_m.txt out_b.txt | head -3
    fi
}
verify "ADD_1M"   moptm_ADD best_ADD test_add_1M_1M.txt
verify "MUL_100k" moptm_MUL best_MUL test_mul_100k_100k.txt
verify "DIV_1M_100k" moptm_DIV best_DIV test_div_1M_100k.txt
verify "DIV_1M_500k" moptm_DIV best_DIV test_div_1M_500k.txt
verify "DIV_1M_900k" moptm_DIV best_DIV test_div_1M_900k.txt
verify "DIV_1M_999k" moptm_DIV best_DIV test_div_1M_999k.txt
