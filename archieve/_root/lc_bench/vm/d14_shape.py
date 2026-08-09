#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D14 形状诊断: 打印各瓶颈用例的 in/nblk/Fi/Fl/Fc/cyc, 定位 lri_03 为何没动。"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
F = "-O2 -std=c++23 -march=x86-64-v3"

put(os.path.join(PS, "best", "div_D14.cpp"), "/home/azzr/divbench/src/d14.cpp")
rc, out, err = run(f"cd ~/divbench/src && g++ {F} -DDIV_MUSHAPE -o ../bin/d14s d14.cpp "
                   f"&& echo BUILT_d14s", timeout=2400)
print(out[-2500:]); print(err[-2000:])
if "BUILT_d14s" not in out:
    sys.exit("build d14s failed")

shape = r'''cd ~/divbench
IN=/home/azzr/lcp/big_integer/division_of_big_integers/in
for c in length_ratio_integer_00 length_ratio_integer_01 length_ratio_integer_02 \
         length_ratio_integer_03 a_max_b_random_02 large_01 max_00; do
  [ -f $IN/$c.in ] || continue
  ./bin/d14s < $IN/$c.in >/dev/null 2>/tmp/sh.txt
  echo "=== $c"
  grep '\[mushape\]' /tmp/sh.txt | sort | uniq -c | sort -rn | head -3
done'''
rc, out2, err = run(shape, timeout=2400)
print(out2)
with open(os.path.join(PS, "lc_bench", "vm", "_d14_shape.log"), "w", encoding="utf-8") as f:
    f.write(out2)
