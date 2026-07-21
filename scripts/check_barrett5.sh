#!/bin/bash
# Verify the actual shr $0xb (S=75 shift) counts
cd /tmp/o2compare

echo "=== Count 'shr \$0xb' (GCC S=75 shift) in each binary ==="
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    cnt=$(objdump -d $f | grep -c 'shr.*\$0xb')
    echo "  $f: shr\$0xb=$cnt"
done

echo ""
echo "=== Count 'mul' (64-bit, used for Barrett high-word) ==="
for f in mf_ADD mf_MUL mf_DIV mf_ADD.bak mf_MUL.bak mf_DIV.bak; do
    cnt=$(objdump -d $f | grep -cE '\bmul\s+%(r|e)?[a-z0-9]+\b')
    echo "  $f: mul=$cnt"
done

echo ""
echo "=== 8-way unrolled loop in fftMul (NEW) - look for 8 consecutive mul patterns ==="
# Find fftMul symbol
sym_addr=$(objdump -t mf_MUL | grep -E 'fftMul.*ViewTy' | head -1 | awk '{print $1}')
echo "fftMul symbol at: 0x$sym_addr"
# Disassemble the whole function
objdump -d --start-address=0x$sym_addr mf_MUL 2>/dev/null | grep -E 'mul|shr.*\$0xb' | head -30
