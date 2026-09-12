#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
ROOT = 'D:/hex_precious_speed'
DB = '/home/azzr/divbench'
SRC = f'{DB}/src/div_v9.cpp'
BIN = f'{DB}/bin/div_v9_asan'

# 1) 重新上传 v9 源码（确保最新）
vmctl.put(os.path.join(ROOT, 'work/div/v9.cpp'), SRC)
# 2) 上传触发 FFT 的输入
vmctl.put(os.path.join(ROOT, 'tools/_d9input.txt'), '/tmp/d9.in')
# 3) ASan 编译
CXX = "g++ -O1 -march=x86-64-v3 -std=c++23 -fsanitize=address -g -fno-omit-frame-pointer"
vmctl.run(f"cd {DB} && {CXX} {SRC} -o {BIN} 2>&1 | tail -20; echo BUILD_RC=${{PIPESTATUS[0]}}")
# 4) 运行 + 抓 ASan 报告
rc, o, _ = vmctl.run(
    f"cd {DB} && {BIN} < /tmp/d9.in > /tmp/d9_out.txt 2> /tmp/d9_asan.txt; echo EXIT=$?; "
    f"echo '--- ASAN (head) ---'; head -40 /tmp/d9_asan.txt; "
    f"echo '--- OUT size ---'; wc -c /tmp/d9_out.txt", timeout=600)
print(o)
