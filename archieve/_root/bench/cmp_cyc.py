#!/usr/bin/env python3
"""远程对比 cyclic vs nocyclic (5 iters 取最小)"""
import sys, os
sys.path.insert(0, r"d:\precious_speed")
from ssh_manager import get_ssh

ssh = get_ssh()

# 编译 nocyclic 版本
rc, out, err = ssh.run(
    "g++ -std=gnu++20 -O2 -static -DONLINE_JUDGE -DHINT_OP_DIV -DDISABLE_2NXN_CYCLIC "
    "/home/azzr/bench_div.cpp -o /home/azzr/bench_div_nocyc -pthread 2>&1", timeout=120)
print(f"nocyclic compile: rc={rc}")
if rc != 0:
    print(out); sys.exit(1)

print("\n=== CYCLIC vs NOCYCLIC (5 iters, min) ===")
print(f"{'label':<10} {'case':<22} {'min_s':<10}")
for label, exe in [("CYCLIC", "/home/azzr/bench_div"), ("NOCYCLIC", "/home/azzr/bench_div_nocyc")]:
    for case in ["div_large_0.in", "div_medium_0.in"]:
        # 用远程 bash 脚本, 避免 Python 内层引号
        script = (
            f"best=999999; "
            f"for i in $(seq 1 5); do "
            f"t=$(/usr/bin/time -f %e {exe} < /tmp/bench_{case} 2>&1 >/dev/null); "
            f"rc=$?; [ $rc -ne 0 ] && echo FAIL:$rc && break; "
            f"python3 -c \"exit(0 if float('$t')<$best else 1)\" 2>/dev/null && best=$t || true; "
            f"done; echo $best"
        )
        rc, out, err = ssh.run(script, timeout=300)
        print(f"{label:<10} {case:<22} {out.strip():<10}")
