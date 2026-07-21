#!/bin/bash
# quick_add_bench.sh
cd /tmp/bench2
echo "=== ADD benchmark (reverted + memcpy opt) ==="
for t in big_add_2M_2M.txt test_add_1M_1M.txt test_add_500k_500k.txt test_add_100k_100k.txt; do
    [ -f "$t" ] || continue
    # warmup
    ./moptm_ADD_bench < "$t" > /dev/null 2>&1
    ./best_ADD < "$t" > /dev/null 2>&1
    # 10 runs avg
    m_total=0; b_total=0
    for i in $(seq 1 10); do
        m=$(./moptm_ADD_bench < "$t" 2>&1 1>/dev/null | awk '/CPU:/{print $2}')
        b=$(./best_ADD < "$t" 2>&1 1>/dev/null | awk '/BEST_CPU:/{print $2}' || true)
        m_total=$(awk -v a=$m_total -v b=$m 'BEGIN{print a+b}')
        b_total=$(awk -v a=$b_total -v b=$b 'BEGIN{print a+b}')
    done
    m_avg=$(awk -v t=$m_total 'BEGIN{printf "%.3f", t/10}')
    b_avg=$(awk -v t=$b_total 'BEGIN{printf "%.3f", t/10}')
    r=$(awk -v m=$m_avg -v b=$b_avg 'BEGIN{printf "%.2f", m/b}')
    echo "  $t: moptm=${m_avg}ms best=${b_avg}ms ratio=${r}x"
done
echo "=== Done ==="
