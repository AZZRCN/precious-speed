#!/bin/bash
# Compile the 7 GEN generators on the VM and produce the 20 canonical .in cases
# matching info.toml distribution. Seeds 0..(number-1) per generator reproduce the
# canonical LC case set (verified: max_max_gen 0 == local canonical max_max_00.in).
set -e
BASE=/home/azzr/mulbench
cd "$BASE/gen/src"
FLAGS="-O2 -std=c++23 -march=x86-64-v3"
for g in max_max large medium small zero fft_killer large_small; do
  echo "compiling ${g}_gen ..."
  g++ $FLAGS -o "../${g}_gen" "${g}.cpp"
done
mkdir -p "$BASE/cases"
cp "$BASE/gen/example_00.in" "$BASE/cases/example_00.in"

../max_max_gen 0     > "$BASE/cases/max_max_00.in"
../max_max_gen 1     > "$BASE/cases/max_max_01.in"
../max_max_gen 2     > "$BASE/cases/max_max_02.in"
../max_max_gen 3     > "$BASE/cases/max_max_03.in"
../max_max_gen 4     > "$BASE/cases/max_max_04.in"
../max_max_gen 5     > "$BASE/cases/max_max_05.in"
../max_max_gen 6     > "$BASE/cases/max_max_06.in"
../max_max_gen 7     > "$BASE/cases/max_max_07.in"
../small_gen 0       > "$BASE/cases/small_00.in"
../medium_gen 0      > "$BASE/cases/medium_00.in"
../medium_gen 1      > "$BASE/cases/medium_01.in"
../medium_gen 2      > "$BASE/cases/medium_02.in"
../large_gen 0       > "$BASE/cases/large_00.in"
../large_gen 1       > "$BASE/cases/large_01.in"
../large_gen 2       > "$BASE/cases/large_02.in"
../zero_gen 0        > "$BASE/cases/zero_00.in"
../fft_killer_gen 0  > "$BASE/cases/fft_killer_00.in"
../fft_killer_gen 1  > "$BASE/cases/fft_killer_01.in"
../large_small_gen 0 > "$BASE/cases/large_small_00.in"

echo "=== generated cases ==="
ls -la "$BASE/cases"
echo "DONE gen cases"
