#!/bin/bash
cd /home/azzr/precious_speed/hex_best
{
  g++-15 -O3 -std=c++20 -march=znver3 -mtune=znver3 -w -DDISABLE_2NXN_CYCLIC -DDISABLE_FFT_ALL -DDIV_MU_INVCHECK -DDIV_MU_INVSELFCHECK -o div_base16_invchk div_base16.cpp
  echo "BUILD_RC=$?"
} > /tmp/invchk_build.txt 2>&1
cd /home/azzr/precious_speed/hex_best
python3 repro_dbg.py ./div_base16_invchk > /tmp/invchk_repro.txt 2>&1
echo "REPRO_RC=$?" >> /tmp/invchk_repro.txt
echo "ALL_DONE" >> /tmp/invchk_all.txt
