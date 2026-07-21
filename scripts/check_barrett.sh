#!/bin/bash
# Verify Barrett constant 0x68DB8BAC710CC in NEW binary vs GCC default 0x346DC5D63886594B in OLD
cd /tmp/o2compare

echo "=== NEW binaries (Barrett-modified) ==="
for f in mf_ADD mf_MUL mf_DIV; do
    total=$(objdump -d $f | grep -E '68db8bac710cc|346dc5d63886594b' | wc -l)
    barrett=$(objdump -d $f | grep -c '68db8bac710cc')
    gcc=$(objdump -d $f | grep -c '346dc5d63886594b')
    echo "  $f: total=$total  barrett(0x68DB..)=$barrett  gcc(0x346D..)=$gcc"
done

echo ""
echo "=== OLD binaries (original) ==="
for f in mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    total=$(objdump -d $f | grep -E '68db8bac710cc|346dc5d63886594b' | wc -l)
    barrett=$(objdump -d $f | grep -c '68db8bac710cc')
    gcc=$(objdump -d $f | grep -c '346dc5d63886594b')
    echo "  $f: total=$total  barrett(0x68DB..)=$barrett  gcc(0x346D..)=$gcc"
done

echo ""
echo "=== shrq \$11 count comparison (GCC default shift) ==="
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    cnt=$(objdump -d $f | grep -c 'shrq.*\$0xb,')
    cnt2=$(objdump -d $f | grep -c 'shrq.*\$11,')
    echo "  $f: shrq0xb=$cnt  shrq11=$cnt2"
done
