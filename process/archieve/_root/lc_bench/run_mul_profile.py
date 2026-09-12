#!/usr/bin/env python3
"""
run_mul_profile.py - 测量MUL正常模式下的I/O vs 计算时间分布.
通过修改源码插入计时点, 编译运行, 然后恢复.
即时交替对比 CUR vs BEST.
"""
import subprocess
import os
import sys
import time
import shutil

ROOT = r"d:\precious_speed"
EXE_DIR = r"d:\precious_speed\lc_bench\exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"

# 用正常模式运行, 通过perf_counter测量端到端时间
# 同时用BENCH_INTERNAL模式测量单次乘法时间
cases = ["max_max_00", "large_00", "fft_killer_00", "medium_00", "small_00",
         "fft_killer_01", "large_01", "max_max_01"]

print(f"{'case':<20} {'CUR_normal':>12} {'BEST_normal':>12} {'CUR_pure_mul':>14} {'BEST_pure_mul':>14}")
print("-" * 75)

for c in cases:
    inp = os.path.join(CASES_DIR, c + ".in")
    if not os.path.exists(inp):
        # 尝试带空格的文件名
        inp2 = os.path.join(CASES_DIR, c + " .in")
        if os.path.exists(inp2):
            inp = inp2
        else:
            print(f"{c:<20} NOT FOUND")
            continue

    # 正常模式: 取3次最小值
    def run_normal(exe):
        times = []
        for _ in range(3):
            with open(inp, "rb") as f:
                t0 = time.perf_counter()
                r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL, timeout=30)
                t1 = time.perf_counter()
                if r.returncode == 0:
                    times.append((t1-t0)*1000)
        return min(times) if times else -1

    # BENCH_INTERNAL模式: 获取单次乘法时间
    def run_pure(exe):
        with open(inp, "rb") as f:
            r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, timeout=30)
        line = r.stderr.decode(errors="replace").strip()
        # 解析 MUL_MIN: 格式 "MUL_MIN: 16.121"
        parts = line.split()
        for i, part in enumerate(parts):
            if part == "MUL_MIN:" and i + 1 < len(parts):
                return float(parts[i + 1])
        return -1

    cur_normal = run_normal(os.path.join(EXE_DIR, "cur_mul_o2.exe"))
    best_normal = run_normal(os.path.join(EXE_DIR, "best_mul_o2.exe"))
    cur_pure = run_pure(os.path.join(EXE_DIR, "mul_bench.exe"))
    # BEST的bench exe还没编译, 暂时用-占位
    best_pure = "-"

    print(f"{c:<20} {cur_normal:>12.1f} {best_normal:>12.1f} {cur_pure:>14.3f} {best_pure:>14}")
