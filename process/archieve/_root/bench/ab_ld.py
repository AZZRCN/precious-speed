#!/usr/bin/env python3
"""精确 A/B 测速: 通过 sed 临时修改源码, 编译 baseline, 对比 opt
仅回退 long double 改动, 保留其他优化 (死代码清理等)
"""
import subprocess, os, shutil, time, sys

ROOT = r"d:\precious_speed"
BENCH_DATA = os.path.join(ROOT, "bench_data")
EXE_DIR = os.path.join(ROOT, "bench", "exe")
os.makedirs(EXE_DIR, exist_ok=True)

CASES = {
    "mul": [("mul_medium_0.in", "med"), ("mul_large_0.in", "large"), ("mul_max_0.in", "max0")],
    "div": [("div_medium_0.in", "med"), ("div_large_0.in", "large"), ("div_max_0.in", "max0"), ("div_max_2.in", "max2")],
}

# long double 改动的标记行 (构造函数 + expand)
LD_MARKERS = [
    "long double theta = -static_cast<long double>(HINT_2PI) * factor / rank;",
    'table[5] = static_cast<Float>(std::cosl(theta)), table[7] = static_cast<Float>(std::sinl(theta));',
    'C2 unit(static_cast<Float>(std::cosl(theta)), static_cast<Float>(std::sinl(theta)));',
    'static thread_local BinRevTableC2HP<Float> table(31, 32);',
]
# 替换回 double 版本
DBL_REPLACEMENTS = [
    "Float theta = -HINT_2PI * factor / rank;",
    "table[5] = std::cos(theta), table[7] = std::sin(theta);",
    'C2 unit(std::cos(theta), std::sin(theta));',
    'BinRevTableC2HP<Float> table(31, 32);',
]


def run(cmd, timeout=60):
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout, shell=isinstance(cmd, str))
    return r.returncode, r.stdout, r.stderr


def patch_file(path, ld_to_dbl):
    """ld_to_dbl=True: long double->double; False: double->long double (restore)"""
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    if ld_to_dbl:
        for ld, dbl in zip(LD_MARKERS, DBL_REPLACEMENTS):
            content = content.replace(ld, dbl)
        # 移除注释行
        content = content.replace("                    // 用 long double 计算三角函数, 减少 twiddle 表累积漂移 (精度优化)\n", "")
    else:
        # 反向: 从当前 double 版本恢复 long double (不实现, 用 git checkout)
        pass
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)


def compile_op(op):
    rc, out, err = run(["ai.bat", f"compile_{op}"], timeout=60)
    if rc != 0:
        print(f"  [ERR] compile {op}: {err[:200]}")
        return None
    exe = os.path.join(ROOT, f"cur_{op}.exe")
    return exe if os.path.exists(exe) else None


def run_once(exe, case_file, timeout=60):
    inp = os.path.join(BENCH_DATA, case_file)
    t0 = time.perf_counter()
    try:
        with open(inp, "rb") as f:
            r = subprocess.run([exe], stdin=f, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=timeout)
        t1 = time.perf_counter()
        return (t1 - t0) * 1000 if r.returncode == 0 else None
    except subprocess.TimeoutExpired:
        return None


def ab_bench(exe_base, exe_opt, case_file, rounds, iters):
    base_times, opt_times = [], []
    for _ in range(rounds):
        ts = [run_once(exe_base, case_file) for _ in range(iters)]
        ts = [t for t in ts if t]
        if ts: base_times.append(min(ts))
        ts = [run_once(exe_opt, case_file) for _ in range(iters)]
        ts = [t for t in ts if t]
        if ts: opt_times.append(min(ts))
    return base_times, opt_times


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--op", default="div")
    ap.add_argument("--iters", type=int, default=5)
    ap.add_argument("--rounds", type=int, default=3)
    args = ap.parse_args()

    op = args.op
    src = os.path.join(ROOT, f"{op}.cpp")
    print(f"\n{'='*60}\n=== A/B Bench (long double isolation): {op} ===\n{'='*60}")

    # 1. 编译 opt (当前 long double 版本)
    print("[1/4] 编译 opt (long double)...")
    exe_opt = compile_op(op)
    if not exe_opt: return
    exe_opt_copy = os.path.join(EXE_DIR, f"opt_{op}.exe")
    shutil.copy(exe_opt, exe_opt_copy)

    # 2. 备份源码, patch 为 double 版本
    print(f"[2/4] patch {op}.cpp -> double baseline...")
    backup = src + ".ld_backup"
    shutil.copy(src, backup)
    patch_file(src, ld_to_dbl=True)

    # 3. 编译 baseline
    print("[3/4] 编译 baseline (double)...")
    exe_base = compile_op(op)
    if not exe_base:
        shutil.copy(backup, src)
        return
    exe_base_copy = os.path.join(EXE_DIR, f"base_{op}.exe")
    shutil.copy(exe_base, exe_base_copy)

    # 4. 恢复源码
    print("[4/4] 恢复 long double 源码...")
    shutil.copy(backup, src)
    os.remove(backup)

    # 5. 交替测速
    print(f"\n--- 交替测速 (rounds={args.rounds}, iters={args.iters}) ---")
    print(f"{'case':<22} {'base_min':>10} {'opt_min':>10} {'delta':>8} {'pct':>8}")
    for cf, label in CASES.get(op, []):
        bt, ot = ab_bench(exe_base_copy, exe_opt_copy, cf, args.rounds, args.iters)
        if not bt or not ot:
            print(f"  {cf:<22} FAIL"); continue
        bmin, omin = min(bt), min(ot)
        delta = omin - bmin
        pct = (delta / bmin) * 100
        print(f"  {cf:<22} {bmin:>9.1f}ms {omin:>9.1f}ms {delta:>+7.1f}ms {pct:>+7.2f}%")


if __name__ == "__main__":
    main()
