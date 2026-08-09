#!/usr/bin/env python3
"""D19 正确性红线: 后台跑 554 例 (26 官方 + 224 定向 + 304 生成器)。

D19 = D17 + 两处 100% 标量热点向量化:
  - absMul1: SWAR 进位链 (仿 D11 absSub_avx2), 16 limb/迭代
  - count_true_length: vptest 尾部清零扫描, 16 limb/迭代

用法:
    python d19_verify.py          # 启动后台验证
    python d19_verify.py poll     # 查看进度
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/d19_verify.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"tail -60 {LOG}; echo '--- alive:'; "
                       f"pgrep -fa d8_verify.py | head -3")
    print(out)
    sys.exit(0)

put(os.path.join(PS, "best", "div_D19.cpp"), "/home/azzr/divbench/src/div_D19.cpp")
put(os.path.join(HERE, "d8_verify.py"), "/home/azzr/d8_verify.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 d8_verify.py div_D19 > {LOG} 2>&1 &  echo started",
    timeout=30)
print("started; poll with: python d19_verify.py poll")
