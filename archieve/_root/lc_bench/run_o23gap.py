# -*- coding: utf-8 -*-
"""量化 D41 的 O2 vs O3 纯编译器差距 (callgrind Ir/cache), 只打最高点族。
用途: 决定「嵌套 radix-21 (~4.5%, 高风险)」与「GCC G O3 预展开 (零算法风险)」哪条更值。
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "vm"))
import vmctl  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SH = os.path.join(HERE, "vm", "_o23gap.sh")

vmctl.put(SH, "/home/azzr/divbench/_o23gap.sh")
rc, out, err = vmctl.run("cd /home/azzr/divbench && sed -i 's/\\r$//' _o23gap.sh && "
                         "bash _o23gap.sh 2>&1", timeout=3000, verbose=False)
print(out)
if err.strip():
    print("---STDERR---")
    print(err[-3000:])
