#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通用 554 例正确性红线验证 —— 取代一次性的 dNN_verify.py。

    python cand_verify.py D24         # 启动后台验证 (26 官方 + 224 定向 + 304 生成器)
    python cand_verify.py D24 poll    # 查看进度

上传 best/div_<C>.cpp -> VM src/, 由 VM 上的 d8_verify.py 逐字节对拍真原版。
日志: /home/azzr/divbench/logs/<c>_verify.log
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))

CAND = sys.argv[1] if len(sys.argv) > 1 else "D24"
LOG = "/home/azzr/divbench/logs/%s_verify.log" % CAND.lower()

if len(sys.argv) > 2 and sys.argv[2] == "poll":
    rc, out, err = run("tail -40 %s; echo '--- alive:'; pgrep -fa d8_verify.py | head -3" % LOG)
    print(out)
    sys.exit(0)

src = os.path.join(PS, "best", "div_%s.cpp" % CAND)
assert os.path.exists(src), src
put(src, "/home/azzr/divbench/src/div_%s.cpp" % CAND)
put(os.path.join(HERE, "d8_verify.py"), "/home/azzr/d8_verify.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run("cd /home/azzr && nohup python3 d8_verify.py div_%s > %s 2>&1 &  echo started" % (CAND, LOG),
    timeout=30)
print("started; poll with: python cand_verify.py %s poll" % CAND)
