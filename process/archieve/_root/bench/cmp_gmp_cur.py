#!/usr/bin/env python3
"""GMP vs CUR 除法测速对比 (VM)"""
import sys, os
sys.path.insert(0, r"d:\precious_speed")
from ssh_manager import get_ssh

ssh = get_ssh()

# 上传 GMP div 程序
gmp_src = r"d:\precious_speed\bench\gmp_div.cpp"
ssh.upload(gmp_src, "/home/azzr/gmp_div.cpp")

# 编译 GMP div
rc, out, err = ssh.run(
    "g++ -O2 -std=gnu++20 /home/azzr/gmp_div.cpp -o /home/azzr/gmp_div -lgmp 2>&1",
    timeout=60)
print(f"GMP div compile: rc={rc}")
if rc != 0:
    print(out); sys.exit(1)

# CUR div 已编译为 /home/azzr/bench_div (cyclic 版本)
# 上传 bench_data (确保最新)
for case in ["div_large_0.in", "div_medium_0.in", "div_max_0.in", "div_max_2.in"]:
    ssh.upload(f"d:/precious_speed/bench_data/{case}", f"/tmp/bench_{case}")

# 测速: GMP vs CUR, 5 iters 取最小
print("\n=== GMP vs CUR DIV (5 iters, min) ===")
print(f"{'prog':<8} {'case':<20} {'min_s':<10}")
for label, exe in [("GMP", "/home/azzr/gmp_div"), ("CUR", "/home/azzr/bench_div")]:
    for case in ["div_large_0.in", "div_medium_0.in", "div_max_0.in", "div_max_2.in"]:
        script = (
            f"best=999999; "
            f"for i in $(seq 1 5); do "
            f"t=$(/usr/bin/time -f %e {exe} < /tmp/bench_{case} 2>&1 >/dev/null); "
            f"rc=$?; [ $rc -ne 0 ] && echo FAIL:$rc && break; "
            f"python3 -c \"exit(0 if float('$t')<$best else 1)\" 2>/dev/null && best=$t || true; "
            f"done; echo $best"
        )
        rc, out, err = ssh.run(script, timeout=300)
        print(f"{label:<8} {case:<20} {out.strip():<10}")
