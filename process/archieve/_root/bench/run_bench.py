#!/usr/bin/env python3
"""
统一测速脚本 - 本地 + SSH远程
用法:
  python bench/run_bench.py                  # 本地测速 (CUR vs BEST, 3次取最小)
  python bench/run_bench.py --pure           # 额外测 pure 计算时间 (无I/O)
  python bench/run_bench.py --remote         # SSH远程测速 (需远程在线)
  python bench/run_bench.py --cases div      # 只测 div
  python bench/run_bench.py --runs 5         # 5次取最小 (默认3)

测速口径:
  - total: wall-clock 从启动到退出 (含 I/O)
  - pure:  BENCH_*_PURE / BENCH_INTERNAL 模式, 仅计算时间 (stderr 输出)
  - 取最小值, 消除 OS 调度抖动
输出: bench/results/bench_<timestamp>.csv
"""
import subprocess, sys, os, time, csv, argparse, statistics
from datetime import datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GXX = r"D:\mingw64\bin\g++.exe"
CXXFLAGS = ["-std=c++20", "-O2", "-I."]
LDFLAGS = ["-lpthread"]

TASKS = {
    "add": {"src": "add.cpp", "macro": "HINT_OP_ADD", "pure": None},
    "mul": {"src": "mul.cpp", "macro": "HINT_OP_MUL", "pure": "BENCH_INTERNAL"},
    "div": {"src": "div.cpp", "macro": "HINT_OP_DIV", "pure": "BENCH_DIV_PURE"},
}

def run_cmd(cmd, cwd=ROOT, timeout=120):
    try:
        p = subprocess.run(cmd, cwd=cwd, capture_output=True, timeout=timeout, text=True)
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "TIMEOUT"

def compile_exe(src, macro, out_path, extra_defines=None):
    defines = [f"-D{macro}"]
    if extra_defines:
        defines += [f"-D{d}" for d in extra_defines]
    cmd = [GXX] + CXXFLAGS + defines + [os.path.join(ROOT, src), "-o", out_path] + LDFLAGS
    rc, _, err = run_cmd(cmd)
    return rc == 0, err

def bench_local(exe, in_path, runs=3):
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        try:
            with open(in_path, 'rb') as fin:
                p = subprocess.run([exe], stdin=fin, capture_output=True, timeout=120)
            t1 = time.perf_counter()
            if p.returncode == 0:
                times.append((t1 - t0) * 1000)
        except subprocess.TimeoutExpired:
            pass
    return min(times) if times else None

def bench_pure_local(exe, in_path, runs=3):
    """测 pure 计算时间, 从 stderr 解析"""
    times = []
    for _ in range(runs):
        try:
            with open(in_path, 'rb') as fin:
                p = subprocess.run([exe], stdin=fin, capture_output=True, timeout=120, text=True)
            if p.returncode == 0 and p.stderr:
                for line in p.stderr.splitlines():
                    if "pure" in line.lower() and "ms" in line.lower():
                        try:
                            val = float(line.split(":")[-1].replace("ms", "").strip())
                            times.append(val)
                            break
                        except ValueError:
                            pass
        except subprocess.TimeoutExpired:
            pass
    return min(times) if times else None

def bench_remote(ssh_mgr, src, macro, in_path, runs=3):
    """SSH远程测速: 上传源码 -> 远