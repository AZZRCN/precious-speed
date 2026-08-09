#!/usr/bin/env python3
"""驱动 d12_fuzz_remote.py: 上传候选 + fuzz 脚本, VM 后台跑, 写日志。

用法:
    python d12_fuzz.py            # 启动 (默认 div_D12)
    python d12_fuzz.py poll       # 查看进度
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "div_D12"
LOG = "/home/azzr/divbench/logs/d12_fuzz.log"
# 阶段过滤 (boundary/nearzero/carry), 逗号分隔; 缺省全跑
STAGES = sys.argv[2] if len(sys.argv) > 2 else "boundary,nearzero,carry"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    run(f"tail -40 {LOG}; echo '--- alive:'; pgrep -fa d12_fuzz_remote | head -3")
    sys.exit(0)

put(os.path.join(PS, "best", f"{NAME}.cpp"), f"/home/azzr/divbench/src/{NAME}.cpp")
put(os.path.join(HERE, "d12_fuzz_remote.py"), "/home/azzr/d12_fuzz_remote.py")
run(f"cd ~/divbench/src && g++ -O2 -std=c++23 -march=x86-64-v3 -o ../bin/{NAME} {NAME}.cpp "
    f"&& echo BUILD_OK", timeout=900)
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 d12_fuzz_remote.py {NAME} {STAGES} > {LOG} 2>&1 & echo started",
    timeout=30)
