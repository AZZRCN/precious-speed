#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""vm_superopt.py — Windows 侧触发: 上传超优化流水线到 VM 并后台跑 (fire-and-forget)。
IP 自动发现由 vmctl 负责 (候选 192.168.1.55 / 10.144.33.157 + 缓存)。

用法:
  python vm_superopt.py            # 默认: 上传+后台跑 D45 在 headline 点
  python vm_superopt.py --poll     # 跑完并打印 superopt.log 尾部
  python vm_superopt.py --top-n 8 --k-max 6 --time-cap 280
"""
import sys, os, time, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vmctl import run, put

HERE = os.path.dirname(os.path.abspath(__file__))
PS = r"D:\precious_speed"
RD = "/home/azzr/divbench"
ISA = r"D:\gcc modifield\_o4_impl\isa\x86_64_zen3.json"
PIPE = os.path.join(HERE, "superopt_pipeline.py")
SRC = os.path.join(PS, "best", "div_D45.cpp")

def upload():
    put(PIPE, RD + "/superopt_pipeline.py")
    put(ISA, RD + "/x86_64_zen3.json")
    put(SRC, RD + "/div_D45.cpp")
    print("[upload] pipeline + isa + div_D45.cpp ->", RD)

def launch(args):
    cmd = ("cd %s && nohup python3 superopt_pipeline.py --src div_D45.cpp "
           "--isa x86_64_zen3.json --case length_ratio_integer_02 "
           "--top-n %d --k-max %d --beam %d --time-cap %.0f "
           "--ckpt-dir superopt_state > superopt.log 2>&1 &"
           % (RD, args.top_n, args.k_max, args.beam, args.time_cap))
    rc, o, e = run(cmd)
    print("[launch] rc=%s (后台 fire-and-forget)" % rc)

def poll(n=40):
    rc, o, e = run("cd %s && tail -n %d superopt.log" % (RD, n))
    print(o)
    if e:
        print("ERR", e[:300])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--poll", action="store_true", help="只查日志")
    ap.add_argument("--top-n", type=int, default=5)
    ap.add_argument("--k-max", type=int, default=5)
    ap.add_argument("--beam", type=int, default=200)
    ap.add_argument("--time-cap", type=float, default=280)
    a = ap.parse_args()
    if a.poll:
        poll()
        return
    upload()
    launch(a)
    time.sleep(3)
    poll(15)

if __name__ == "__main__":
    main()
