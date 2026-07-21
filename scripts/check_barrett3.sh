#!/bin/bash
# Detailed check of GCC's default reduction in OLD binary
cd /tmp/o2compare

echo "=== Context around GCC constant 0x346DC5D63886594B in mf_MUL.bak ==="
objdump -d mf_MUL.bak | grep -B 2 -A 5 '346dc5d63886594b' | head -60
echo ""
echo "=== All 'shrq' or 'sarq' or 'shrd' in mf_MUL.bak ==="
objdump -d mf_MUL.bak | grep -E 'shrq|sarq|shrd' | head -30
echo ""
echo "=== All 'shrq' or 'sarq' or 'shrd' in mf_MUL (NEW) ==="
objdump -d mf_MUL | grep -E 'shrq|sarq|shrd' | head -30
echo ""
echo "=== imulq count ==="
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    cnt=$(objdump -d $f | grep -c 'imulq')
    echo "  $f: imulq=$cnt"
done
