#!/usr/bin/env python3
"""D12 正确性验证驱动: 上传 div_D12.cpp + d8_verify.py, 后台跑 554 例, 写日志。

用法:
    python d12_verify.py          # 启动后台验证
    python d12_verify.py poll     # 查看进度
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/d12_verify.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    run(f"tail -40 {LOG}; echo '--- alive:'; pgrep -fa d8_verify.py | head -3")
    sys.exit(0)

put(os.path.join(PS, "best", "div_D12.cpp"), "/home/azzr/divbench/src/div_D12.cpp")
put(os.path.join(HERE, "d8_verify.py"), "/home/azzr/d8_verify.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 d8_verify.py div_D12 > {LOG} 2>&1 &  echo started",
    timeout=30)
