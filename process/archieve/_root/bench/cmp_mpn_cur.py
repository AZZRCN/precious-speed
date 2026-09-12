#!/usr/bin/env python3
"""GMP mpn 纯除法 vs CUR 纯除法 (跳过 I/O) 对比
口径: GMP 用 mpn_tdiv_qr (内部分层调度), CUR 用 BENCH_DIV_PURE (parse+div+write 总时减去 parse+write)
结论决定: 是否值得超优化进制转换函数 (若 GMP mpn 远快于 CUR pure div, 则进制转换是 CUR 唯一可借 GMP 之处的反向证明)
"""
import sys, os
sys.path.insert(0, r"d:\precious_speed")
from ssh_manager import get_ssh

ssh = get_ssh()

# 上传 GMP mpn 基准
ssh.upload(r"d:\precious_speed\bench\bench_mpn_div.cpp", "/home/azzr/bench_mpn_div.cpp")

# 编译 GMP mpn 基准
rc, out, err = ssh.run(
    "g++ -O2 -std=gnu++20 /home/azzr/bench_mpn_div.cpp -o /home/azzr/bench_mpn_div -lgmp 2>&1",
    timeout=60)
print(f"GMP mpn div compile: rc={rc}")
if rc != 0:
    print(out); sys.exit(1)

# 上传 CUR div.cpp (含 cyclic) 并编译 BENCH_DIV_PURE 版本
ssh.upload(r"d:\precious_speed\div.cpp", "/home/azzr/cur_div_pure.cpp")
rc, out, err = ssh.run(
    "g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DBENCH_DIV_PURE "
    "/home/azzr/cur_div_pure.cpp -o /home/azzr/cur_div_pure -pthread 2>&1",
    timeout=120)
print(f"CUR pure div compile: rc={rc}")
if rc != 0:
    print(out); sys.exit(1)

# 确保 bench_data 上传
for case in ["div_large_0.in", "div_medium_0.in", "div_max_0.in", "div_max_2.in"]:
    ssh.upload(f"d:/precious_speed/bench_data/{case}", f"/tmp/bench_{case}")

# 测速
# GMP mpn: 程序输出平均每次 ms (已内部计时 iters 次)
# CUR pure: 直接运行, stderr 含 "pure div time: X ms"; 另用 /usr/bin/time 取 wall time
print("\n=== GMP mpn (pure) vs CUR pure div ===")
print(f"{'prog':<14} {'case':<20} {'ms':<12}")
print("-" * 48)

for case in ["div_large_0.in", "div_medium_0.in", "div_max_0.in", "div_max_2.in"]:
    # GMP mpn: 10 iters 内部计时, 输出平均每次 ms
    rc, out_g, err_g = ssh.run(f"/home/azzr/bench_mpn_div /tmp/bench_{case} 10", timeout=300)
    gmp_ms = "?"
    if rc == 0:
        lines = [l for l in out_g.strip().splitlines() if l.strip()]
        gmp_ms = lines[-1] if lines else "empty"
    else:
        gmp_ms = f"FAIL(rc={rc}:{err_g[:60]})"

    # CUR pure: 直接运行, stderr 含 pure div time; 取 3 次最小
    cur_pure_ms = "?"
    cur_times = []
    import re
    for _ in range(3):
        rc, out_c, err_c = ssh.run(
            f"/home/azzr/cur_div_pure < /tmp/bench_{case} 2>&1 >/dev/null",
            timeout=120)
        for line in (out_c or "").splitlines():
            m = re.search(r"pure div time:\s*([\d.]+)\s*ms", line)
            if m:
                cur_times.append(float(m.group(1)))
                break
    if cur_times:
        cur_pure_ms = f"{min(cur_times):.1f}"
    else:
        cur_pure_ms = f"PARSE_FAIL({out_c[:60]})"

    print(f"{'GMP mpn':<14} {case:<20} {gmp_ms:<12}")
    print(f"{'CUR pure':<14} {case:<20} {cur_pure_ms:<12}")
    print()
