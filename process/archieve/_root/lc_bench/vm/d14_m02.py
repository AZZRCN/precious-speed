#!/usr/bin/env python3
"""抓 medium_02 的 [unwrap] 明细 + 输入规模。"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run  # noqa: E402

PS = r"D:\precious_speed"
IN = "/home/azzr/lcp/big_integer/division_of_big_integers/in/"

cmd = (
    f"cd ~/divbench && head -c 200 {IN}medium_02.in | head -2 | cut -c1-60; "
    f"echo '--- sizes (digits per line) ---'; "
    f"awk '{{print NR\": \"length($0)}}' {IN}medium_02.in | head -4; "
    f"echo '--- probe g2m0 ---'; "
    f"./bin/g2m0 < {IN}medium_02.in > /tmp/m02.out 2>/tmp/m02.err; "
    f"grep -c '' /tmp/m02.err; cat /tmp/m02.err; "
    f"echo '--- probe g3m0 ---'; "
    f"./bin/g3m0 < {IN}medium_02.in > /dev/null 2>/tmp/m02b.err; cat /tmp/m02b.err"
)
rc, out, err = run(cmd, timeout=1800)
print(out)
print(err[-2000:])
with open(os.path.join(PS, "lc_bench", "vm", "_m02_probe.log"), "w", encoding="utf-8") as f:
    f.write(out)
