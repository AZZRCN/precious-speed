#!/bin/bash
# debug_chrono.sh
cd /tmp/bench2

echo "=== Test 1: direct run ==="
./moptm_ADD_bench < test_add_1M_1M.txt > /dev/null
echo "exit code: $?"

echo ""
echo "=== Test 2: capture stderr ==="
./moptm_ADD_bench < test_add_1M_1M.txt 2>/tmp/err.txt > /dev/null
echo "stderr content:"
cat /tmp/err.txt

echo ""
echo "=== Test 3: pipe stderr ==="
t=$(./moptm_ADD_bench < test_add_1M_1M.txt 2>&1 1>/dev/null)
echo "captured: [$t]"

echo ""
echo "=== Test 4: awk extract ==="
t=$(./moptm_ADD_bench < test_add_1M_1M.txt 2>&1 1>/dev/null | awk '/CPU:/{print $2}')
echo "awk result: [$t]"

echo ""
echo "=== Test 5: 3 runs ==="
total=0
for i in 1 2 3; do
    t=$(./moptm_ADD_bench < test_add_1M_1M.txt 2>&1 1>/dev/null | awk '/CPU:/{print $2}')
    echo "  run $i: t=$t"
    total=$(awk -v tot=$total -v t=$t 'BEGIN{print tot + t}')
done
echo "total=$total, avg=$(awk -v tot=$total 'BEGIN{print tot/3}')"
