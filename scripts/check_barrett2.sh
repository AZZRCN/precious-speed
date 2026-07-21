#!/bin/bash
# Detailed assembly check - find all shrq instructions and Barrett-related patterns
cd /tmp/o2compare

echo "=== All shrq instructions in mf_MUL (NEW) ==="
objdump -d mf_MUL | grep -E 'shrq' | head -30
echo ""
echo "=== All shrq instructions in mf_MUL.bak (OLD) ==="
objdump -d mf_MUL.bak | grep -E 'shrq' | head -30
echo ""
echo "=== All mulq instructions count (NEW vs OLD) ==="
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    cnt=$(objdump -d $f | grep -c 'mulq')
    echo "  $f: mulq=$cnt"
done
echo ""
echo "=== Context around Barrett constant in mf_MUL ==="
objdump -d mf_MUL | grep -B 2 -A 2 '68db8bac710cc' | head -40
