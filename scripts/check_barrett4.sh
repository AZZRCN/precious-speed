#!/bin/bash
# Dump actual assembly in fftMul carry chain
cd /tmp/o2compare

echo "=== fftMul function disassembly around Barrett constant (NEW) ==="
# Find address where Barrett constant is loaded in fftMul
addr=$(objdump -d mf_MUL | grep -B 1 '68db8bac710cc' | tail -2 | head -1 | awk '{print $1}' | tr -d ':')
echo "Barrett constant loaded at address: $addr"
# Disassemble 100 instructions starting from there
objdump -d --start-address=0x${addr} --stop-address=0x$(printf '%x' $((0x${addr} + 0x300))) mf_MUL | head -80

echo ""
echo "=== Same for OLD binary ==="
addr_old=$(objdump -d mf_MUL.bak | grep -B 1 '346dc5d63886594b' | tail -2 | head -1 | awk '{print $1}' | tr -d ':')
echo "GCC constant loaded at address: $addr_old"
objdump -d --start-address=0x${addr_old} --stop-address=0x$(printf '%x' $((0x${addr_old} + 0x300))) mf_MUL.bak | head -80

echo ""
echo "=== All multiplication-related instructions (mul, imul, mulx) ==="
for f in mf_MUL mf_MUL.bak; do
    echo "--- $f ---"
    objdump -d $f | grep -E '\bmul\b|\bmulq\b|\bimul\b|\bimulq\b|\bmulx\b|\bmulxq\b' | head -20
done
