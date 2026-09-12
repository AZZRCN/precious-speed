#!/usr/bin/env python3
"""交替测速: 对比优化前后版本, 消除时间差异
流程:
  1. 编译当前版本(opt), 复制 exe 到 bench/exe/opt/
  2. git stash push mul.cpp div.cpp 回退 baseline
  3. 编译 baseline, 复制 exe 到 bench/exe/base/
  4. git stash pop 恢复 opt 源码
  5. 交替运行 base/opt 多轮, 输出对比表
用法: python bench/ab_bench.py --op div --iters 5 --rounds 3
"""
import argparse, subprocess, time, os, shutil, sys, statistics

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BENCH_DATA = os.path.join(ROOT, "bench_data")
EXE_DIR = os.path.join(ROOT, "bench", "exe")
os.makedirs(EXE_DIR, exist_ok=True)

CASES = {
    "add": [("add_medium_0.in", "med0"), ("add_large_0.in", "large"), ("add_max_0.in", "max0")],
    "mul": [("mul_medium_0.in", "med"), ("mul_large_0.in", "large"), ("mul_max_0.in", "max0")],
    "div": [("div_medium_0.in", "med"), ("div_large_0.in", "large"), ("div_max_0.in", "max0"), ("div_max_2.in", "max2")],
}


def run(cmd, cwd=ROOT, timeout=60, capture=True):
    r = subprocess.run(cmd, cwd=cwd, capture_output=capture, text=True, timeout=timeout, shell=isinstance(cmd, str))
    return r.returncode, r.stdout, r.stderr


def compile_op(op):
    """编译指定 op, 返回 exe 路径"""
    rc, out, err = run(["ai.bat", f"compile_{op}"], timeout=60)
    if rc != 0:
        print(f"[ERR] compile {op} failed: {err[:200]}")
        return None
    exe = os.path.join(ROOT, f"cur_{op}.exe")
    return exe if os.path.exists(exe) else None


def run_once(exe, case_file, timeout=60):
    """运行一次, 返回 ms 或 None"""
    inp = os.path.join(BENCH_DATA, case_file)
    if not os.path.exists(exe) or not os.path.exists(inp):
        return None
    t0 = time.perf_counter()
    try:
        with open(inp, "rb") as f:
            r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=timeout)
        t1 = time.perf_counter()
        return (t1 - t0) * 1000 if r.returncode == 0 else None
    except subprocess.TimeoutExpired:
        return None


def ab_bench(exe_base, exe_opt, case_file, rounds, iters):
    """交替测速: base, opt, base, opt, ... 每轮 iters 次取最小"""
    base_times, opt_times = [], []
    for r in range(rounds):
        # 先 base
        ts = [run_once(exe_base, case_file) for _ in range(iters)]
        ts = [t for t in ts if t is not None]
        if ts:
            base_times.append(min(ts))
        # 后 opt
        ts = [run_once(exe_opt, case_file) for _ in range(iters)]
        ts = [t for t in ts if t is not None]
        if ts:
            opt_times.append(min(ts))
    return base_times, opt_times


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--op", default="div", help="add|mul|div|all")
    ap.add_argument("--iters", type=int, default=5, help="每轮运行次数")
    ap.add_argument("--rounds", type=int, default=3, help="交替轮数")
    args = ap.parse_args()

    ops = ["add", "mul", "div"] if args.op == "all" else [args.op]

    for op in ops:
        print(f"\n{'='*60}\n=== A/B Bench: {op} (rounds={args.rounds}, iters={args.iters}) ===\n{'='*60}")

        # 1. 编译当前 opt 版本
        print(f"[1/4] 编译 opt 版本 ({op}.cpp 当前状态)...")
        exe_opt = compile_op(op)
        if not exe_opt:
            continue
        exe_opt_copy = os.path.join(EXE_DIR, f"opt_{op}.exe")
        shutil.copy(exe_opt, exe_opt_copy)
        print(f"  opt exe: {exe_opt_copy}")

        # 2. git stash 回退 mul.cpp/div.cpp (仅指定文件)
        print(f"[2/4] git stash 回退 {op}.cpp 到 baseline...")
        rc, out, err = run(["git", "stash", "push", "--", f"{op}.cpp"], timeout=30)
        if rc != 0:
            print(f"  git stash 失败: {err[:200]}")
            continue
        stashed = True

        # 3. 编译 baseline
        print(f"[3/4] 编译 baseline 版本...")
        exe_base = compile_op(op)
        if not exe_base:
            run(["git", "stash", "pop"], timeout=30)
            continue
        exe_base_copy = os.path.join(EXE_DIR, f"base_{op}.exe")
        shutil.copy(exe_base, exe_base_copy)
        print(f"  base exe: {exe_base_copy}")

        # 4. git stash pop 恢复
        print(f"[4/4] git stash pop 恢复 opt 源码...")
        run(["git", "stash", "pop"], timeout=30)

        # 5. 交替测速
        print(f"\n--- 交替测速 ---")
        print(f"{'case':<22} {'base_min':>10} {'opt_min':>10} {'delta':>8} {'pct':>8}")
        for cf, label in CASES.get(op, []):
            base_times, opt_times = ab_bench(exe_base_copy, exe_opt_copy, cf, args.rounds, args.iters)
            if not base_times or not opt_times:
                print(f"  {cf:<22} FAIL")
                continue
            bmin, omin = min(base_times), min(opt_times)
            delta = omin - bmin
            pct = (delta / bmin) * 100
            print(f"  {cf:<22} {bmin:>9.1f}ms {omin:>9.1f}ms {delta:>+7.1f}ms {pct:>+7.2f}%")


if __name__ == "__main__":
    main()
