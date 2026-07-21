#!/bin/bash
# Final verification: count specific instruction patterns in the carry chain
cd /tmp/o2compare

echo "=== Instruction count comparison (per binary) ==="
echo ""
echo "Binary       | mul(64b) | shr\$0xb | imul\$0x2710 | total insns"
echo "-------------|----------|---------|-------------|------------"
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    mul_cnt=$(objdump -d $f | grep -cE '\bmul\s+%r')
    shr_cnt=$(objdump -d $f | grep -c 'shr.*\$0xb')
    imul_cnt=$(objdump -d $f | grep -c 'imul.*\$0x2710')
    total=$(objdump -d $f | wc -l)
    printf "%-12s | %8d | %7d | %11d | %d\n" "$f" "$mul_cnt" "$shr_cnt" "$imul_cnt" "$total"
done

echo ""
echo "=== Verify: NEW uses 0x68DB8BAC710CC, OLD uses 0x346DC5D63886594B ==="
echo "Binary       | 0x68DB.. (Barrett) | 0x346D.. (GCC default)"
echo "-------------|-------------------|---------------------"
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    barrett=$(objdump -d $f | grep -c '68db8bac710cc')
    gcc=$(objdump -d $f | grep -c '346dc5d63886594b')
    printf "%-12s | %17d | %19d\n" "$f" "$barrett" "$gcc"
done
