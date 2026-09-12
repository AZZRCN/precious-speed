#!/bin/bash
cd /home/azzr/precious_speed/hex_best
{
  g++-15 -O3 -std=c++20 -march=znver3 -mtune=znver3 -w -DDISABLE_2NXN_CYCLIC -DDISABLE_FFT_ALL -o div_base16_allexact div_base16.cpp
  echo "BUILD_RC=$?"
} > /tmp/allexact_build.txt 2>&1
cd /home/azzr/precious_speed/hex_best
python3 repro.py ./div_base16_allexact > /tmp/allexact_repro.txt 2>&1
echo "REPRO_RC=$?" >> /tmp/allexact_repro.txt
echo "ALL_DONE" >> /tmp/allexact_all.txt
