#!/usr/bin/env python3
"""
ab_mul_writeopt.py - A/B test: writeTo high-limb lookup optimization.
即时交替对比优化前/后, 不依赖记录的baseline.
"""
import subprocess
import os
import time
import shutil

ROOT = r"d:\precious_speed"
EXE_DIR = r"d:\precious_speed\lc_bench\exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases\mul"
SRC = os.path.join(ROOT, "mul.cpp")
BACKUP = SRC + ".writeopt_backup"

# 用例集: 覆盖大/中/小规模
CASES = [
    "max_max_00", "max_max_01", "large_00", "large_01",
    "fft_killer_00", "fft_killer_01",
    "medium_00", "medium_01", "medium_02",
    "small_00", "zero_00", "large_small_00",
    "example_00",
]

ROUNDS = 4  # 交替轮数


def compile_exe(tag, extra_flags=""):
    """编译mul.cpp, 返回exe路径."""
    exe = os.path.join(EXE_DIR, f"mul_{tag}.exe")
    cmd = f'g++ -std=c++20 -O2 -march=native -I. {extra_flags} "{SRC}" -o "{exe}" -lpthread'
    r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        print(f"COMPILE FAIL ({tag}): {r.stderr[:300]}")
        return None
    return exe


def run_case(exe, case_name, iters=3):
    """运行用例多次, 取最小值."""
    # 尝试带空格和不带空格的文件名
    inp = os.path.join(CASES_DIR, case_name + ".in")
    if not os.path.exists(inp):
        inp = os.path.join(CASES_DIR, case_name + " .in")
    if not os.path.exists(inp):
        return None
    times = []
    for _ in range(iters):
        with open(inp, "rb") as f:
            t0 = time.perf_counter()
            r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=30)
            t1 = time.perf_counter()
            if r.returncode == 0:
                times.append((t1 - t0) * 1000)
    return min(times) if times else None


def main():
    # 1. 备份当前源码 (已含优化)
    shutil.copy(SRC, BACKUP)

    # 2. 编译优化版
    print("[1/3] Compiling OPT (writeTo lookup)...")
    exe_opt = compile_exe("writeopt")
    if not exe_opt:
        shutil.copy(BACKUP, SRC)
        return

    # 3. 恢复原版 (循环除法) 并编译
    print("[2/3] Compiling BASE (loop div)...")
    with open(SRC, 'r', encoding='utf-8') as f:
        content = f.read()
    # 替换优化版为原版
    opt_block = """            // 最高位 limb 不补前导零 (查表替代循环除法)
            uint16_t high = data.back();
            if (high < 10) {
                *p++ = char('0' + high);
            } else if (high < 100) {
                *p++ = char('0' + high / 10);
                *p++ = char('0' + high % 10);
            } else if (high < 1000) {
                *p++ = char('0' + high / 100);
                *p++ = char('0' + (high / 10) % 10);
                *p++ = char('0' + high % 10);
            } else {
                std::memcpy(p, &outTable.t[high], 4);
                p += 4;
            }"""
    base_block = """            // 最高位 limb 不补前导零
            uint16_t high = data.back();
            char tmp[5];
            int n = 0;
            do
            {
                tmp[n++] = char('0' + high % 10);
                high /= 10;
            } while (high);
            while (n--)
            {
                *p++ = tmp[n];
            }"""
    content = content.replace(opt_block, base_block)
    with open(SRC, 'w', encoding='utf-8') as f:
        f.write(content)
    exe_base = compile_exe("baseline")
    if not exe_base:
        shutil.copy(BACKUP, SRC)
        return

    # 4. 恢复优化版
    print("[3/3] Restoring OPT source...")
    shutil.copy(BACKUP, SRC)
    os.remove(BACKUP)

    # 5. 即时交替测速
    print(f"\n{'case':<20} {'BASE':>10} {'OPT':>10} {'diff':>8} {'diff%':>7}")
    print("-" * 60)
    for c in CASES:
        # 交替运行: BASE, OPT, BASE, OPT, ...
        b_times = []
        o_times = []
        for _ in range(ROUNDS):
            t = run_case(exe_base, c)
            if t: b_times.append(t)
            t = run_case(exe_opt, c)
            if t: o_times.append(t)
        b_min = min(b_times) if b_times else 0
        o_min = min(o_times) if o_times else 0
        diff = o_min - b_min
        pct = (diff / b_min * 100) if b_min > 0 else 0
        flag = " *" if abs(pct) > 2 else ""
        print(f"{c:<20} {b_min:>10.1f} {o_min:>10.1f} {diff:>+8.1f} {pct:>+7.1f}%{flag}")


if __name__ == "__main__":
    main()
