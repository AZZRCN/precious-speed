#!/usr/bin/env python3
"""非阻塞 GMP mpn vs CUR pure div 测速
用法: python run_bench_nb.py launch  -> 后台启动
      python run_bench_nb.py poll    -> 查看进度
      python run_bench_nb.py fetch   -> 取回结果
"""
import sys, os, time
sys.path.insert(0, r"d:\precious_speed")
from ssh_manager import get_ssh

ssh = get_ssh()
if ssh.host is None:
    print("[ERR] no reachable host"); sys.exit(1)

REMOTE_SCRIPT = "/home/azzr/bench_nb.sh"
REMOTE_LOG = "/home/azzr/bench_nb.log"
REMOTE_PID = "/home/azzr/bench_nb.pid"

# 远程 shell 脚本: 编译+测速, 全部写入日志
SCRIPT_BODY = r"""#!/bin/bash
set -e
LOG=/home/azzr/bench_nb.log
echo "=== START $(date) ===" > $LOG

# 编译 GMP mpn 基准
g++ -O2 -std=gnu++20 /home/azzr/bench_mpn_div.cpp -o /home/azzr/bench_mpn_div -lgmp >> $LOG 2>&1
echo "[compile] gmp_mpn OK" >> $LOG

# 编译 CUR pure div
g++ -O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV -DBENCH_DIV_PURE \
    /home/azzr/cur_div_pure.cpp -o /home/azzr/cur_div_pure -pthread >> $LOG 2>&1
echo "[compile] cur_pure OK" >> $LOG

for case in div_large_0.in div_medium_0.in div_max_0.in div_max_2.in; do
    echo "--- $case ---" >> $LOG
    # GMP mpn: 10 iters, 输出平均 ms
    g=$(/home/azzr/bench_mpn_div /tmp/bench_$case 10 2>>$LOG)
    echo "GMP_mpn $case $g ms" >> $LOG
    # CUR pure: 3 次取最小
    best=999999
    for i in 1 2 3; do
        t=$(/home/azzr/cur_div_pure < /tmp/bench_$case 2>>$LOG >/dev/null)
        # 从 stderr 抓 pure div time
        pt=$(grep -oP 'pure div time:\s*\K[\d.]+' $LOG | tail -1)
        [ -n "$pt" ] && python3 -c "exit(0 if float('$pt')<$best else 1)" && best=$pt
    done
    echo "CUR_pure $case $best ms" >> $LOG
done
echo "=== DONE $(date) ===" >> $LOG
"""

def launch():
    # 上传源码
    ssh.upload(r"d:\precious_speed\bench\bench_mpn_div.cpp", "/home/azzr/bench_mpn_div.cpp")
    ssh.upload(r"d:\precious_speed\div.cpp", "/home/azzr/cur_div_pure.cpp")
    for case in ["div_large_0.in", "div_medium_0.in", "div_max_0.in", "div_max_2.in"]:
        ssh.upload(f"d:/precious_speed/bench_data/{case}", f"/tmp/bench_{case}")
    # 写脚本
    ssh.run(f"cat > {REMOTE_SCRIPT} << 'EOF_BASH'\n{SCRIPT_BODY}\nEOF_BASH\nchmod +x {REMOTE_SCRIPT}")
    # 后台启动
    pid, _ = ssh.run_bg(f"bash {REMOTE_SCRIPT}", out_file=REMOTE_LOG)
    # 记录 PID
    ssh.run(f"echo {pid} > {REMOTE_PID}")
    print(f"[launch] PID={pid} log={REMOTE_LOG}")
    print("[next] poll: python run_bench_nb.py poll")

def poll():
    rc, out, err = ssh.run(f"kill -0 $(cat {REMOTE_PID} 2>/dev/null) 2>/dev/null && echo RUNNING || echo DONE", timeout=10)
    status = "RUNNING" if "RUNNING" in out else "DONE"
    rc, out, err = ssh.run(f"tail -n 20 {REMOTE_LOG}", timeout=10)
    print(f"[poll] {status}")
    print(out)

def fetch():
    rc, out, err = ssh.run(f"cat {REMOTE_LOG}", timeout=30)
    print(out)

if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "launch"
    if cmd == "launch": launch()
    elif cmd == "poll": poll()
    elif cmd == "fetch": fetch()
    else: print("usage: launch|poll|fetch")
