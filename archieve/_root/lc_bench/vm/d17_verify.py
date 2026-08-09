#!/usr/bin/env python3
"""D17 正确性红线: 后台跑 554 例 (26 官方 + 224 定向 + 304 生成器)。

D17 = D16 + FFTW 式 codelet:
  - split-radix 递归基例 float_len<=8 -> <=FFT_FIXED_MAX(=32), 子树编译期展开
  - 递归内冗余 expand() 移除 (外部入口已按最大长度 expand, 表单调增长)

用法:
    python d17_verify.py          # 启动后台验证
    python d17_verify.py poll     # 查看进度
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import put, run  # noqa: E402

PS = r"D:\precious_speed"
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = "/home/azzr/divbench/logs/d17_verify.log"

if len(sys.argv) > 1 and sys.argv[1] == "poll":
    rc, out, err = run(f"tail -50 {LOG}; echo '--- alive:'; "
                       f"pgrep -fa d8_verify.py | head -3")
    print(out)
    sys.exit(0)

put(os.path.join(PS, "best", "div_D17.cpp"), "/home/azzr/divbench/src/div_D17.cpp")
put(os.path.join(HERE, "d8_verify.py"), "/home/azzr/d8_verify.py")
run("mkdir -p /home/azzr/divbench/logs && rm -f " + LOG)
run(f"cd /home/azzr && nohup python3 d8_verify.py div_D17 > {LOG} 2>&1 &  echo started",
    timeout=30)
print("started; poll with: python d17_verify.py poll")
