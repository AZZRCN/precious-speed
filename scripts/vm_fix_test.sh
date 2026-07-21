#!/bin/bash
# Fix data files: prepend "1" (test case count) and verify
set +e
cd /tmp/bench

echo "=== Fixing data files (prepend count=1) ==="
for f in add_1M.txt mul_500k.txt div_1M_500k.txt; do
    { echo 1; cat "$f"; } > "${f}.new" && mv "${f}.new" "$f"
    echo "  $f: $(wc -l < $f) lines, $(wc -c < $f) bytes"
done

echo ""
echo "=== Small correctness test ==="
printf '1\n123\n456\n' > /tmp/small_in.txt
echo "Input:"; cat /tmp/small_in.txt

echo "--- ADD (expect 579) ---"
for bin in moptm_O2_ADD fusion_O2_ADD fusion_o2only_O2_ADD best_O2_ADD; do
    out=$(./$bin < /tmp/small_in.txt 2>&1)
    printf "  %-30s -> %s\n" "$bin" "$out"
done

echo "--- MUL (expect 56088) ---"
for bin in moptm_O2_MUL fusion_O2_MUL fusion_o2only_O2_MUL best_O2_MUL; do
    out=$(./$bin < /tmp/small_in.txt 2>&1)
    printf "  %-30s -> %s\n" "$bin" "$out"
done

echo "--- DIV (expect '0 123') ---"
for bin in moptm_O2_DIV fusion_O2_DIV fusion_o2only_O2_DIV best_O2_DIV; do
    out=$(./$bin < /tmp/small_in.txt 2>&1)
    printf "  %-30s -> %s\n" "$bin" "$out"
done

echo ""
echo "=== Done ==="
