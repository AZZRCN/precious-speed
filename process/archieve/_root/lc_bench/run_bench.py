#!/usr/bin/env python3
"""
run_bench.py - LC official cases A/B/C/D bench (CUR_O2, CUR_O3, BEST_O2, BEST_O3)
即时交替实测: 每轮4个版本轮流跑, 消除系统性能波动.
不依赖任何记录的baseline, 全部即时交替测量.
"""
import subprocess
import time
import os
import csv
import sys
from pathlib import Path
from datetime import datetime

EXE_DIR = r"d:\precious_speed\lc_bench\exe"
CASES_DIR = r"d:\precious_speed\lc_bench\cases"
RESULTS_DIR = r"d:\precious_speed\lc_bench\results"

# 4 versions per op, order for alternating (interleave CUR/BEST to avoid cache bias)
VERSIONS = {
    "add": ["cur_add_o2", "best_add_o2", "cur_add_o3", "best_add_o3"],
    "mul": ["cur_mul_o2", "best_mul_o2", "cur_mul_o3", "best_mul_o3"],
    "div": ["cur_div_o2", "best_div_o2", "cur_div_o3", "best_div_o3"],
}

WARMUP = 1       # 热身轮数
ROUNDS = 3       # 测速轮数 (取最小值)
TIMEOUT = 15     # 单次运行超时(秒)


def run_once(exe_path, inp_path, timeout=TIMEOUT):
    """Run exe with input, return elapsed ms or None on failure."""
    try:
        with open(inp_path, "rb") as f:
            t0 = time.perf_counter()
            r = subprocess.run(
                [exe_path],
                stdin=f,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=timeout,
            )
            t1 = time.perf_counter()
        if r.returncode == 0:
            return (t1 - t0) * 1000.0
        return None
    except subprocess.TimeoutExpired:
        return "TIMEOUT"
    except Exception as e:
        return f"ERR:{e}"


def bench_case(op, case_file, versions):
    """对单个用例交替测速: 热身 + ROUNDS轮交替."""
    inp = os.path.join(CASES_DIR, op, case_file)
    exe_paths = {v: os.path.join(EXE_DIR, v + ".exe") for v in versions}

    # 检查exe是否存在
    for v, p in exe_paths.items():
        if not os.path.exists(p):
            return {v: "NO_EXE" for v in versions}

    results = {v: [] for v in versions}

    # 热身: 4个版本轮流跑1次 (结果不计入)
    for v in versions:
        run_once(exe_paths[v], inp)

    # 交替测速: 每轮4个版本依次跑
    for _ in range(ROUNDS):
        for v in versions:
            t = run_once(exe_paths[v], inp)
            if isinstance(t, float):
                results[v].append(t)

    # 取每个版本的最小值
    return {v: (min(results[v]) if results[v] else "FAIL") for v in versions}


def format_ms(val):
    if isinstance(val, float):
        return f"{val:.1f}"
    return str(val)


def main():
    ops_to_run = sys.argv[1:] if len(sys.argv) > 1 else ["add", "mul", "div"]

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = os.path.join(RESULTS_DIR, f"lc_bench_{timestamp}.csv")

    all_rows = []

    for op in ops_to_run:
        if op not in VERSIONS:
            continue
        versions = VERSIONS[op]
        cases_dir = os.path.join(CASES_DIR, op)
        case_files = sorted(
            f for f in os.listdir(cases_dir) if f.endswith(".in")
        )

        print(f"\n{'='*70}")
        print(f"=== {op.upper()} : {len(case_files)} cases ===")
        print(f"{'='*70}")
        header = f"{'case':<35} {'CUR_O2':>10} {'BEST_O2':>10} {'CUR_O3':>10} {'BEST_O3':>10}  {'C2/B2':>7} {'C3/B3':>7}"
        print(header)
        print("-" * len(header))

        for cf in case_files:
            res = bench_case(op, cf, versions)
            v_cur_o2 = res["cur_" + op + "_o2"]
            v_best_o2 = res["best_" + op + "_o2"]
            v_cur_o3 = res["cur_" + op + "_o3"]
            v_best_o3 = res["best_" + op + "_o3"]

            # 比值 (CUR/BEST, <1表示CUR更快)
            def ratio(a, b):
                if isinstance(a, float) and isinstance(b, float) and b > 0:
                    return f"{a/b:.3f}"
                return "-"
            r_o2 = ratio(v_cur_o2, v_best_o2)
            r_o3 = ratio(v_cur_o3, v_best_o3)

            print(f"{cf:<35} {format_ms(v_cur_o2):>10} {format_ms(v_best_o2):>10} "
                  f"{format_ms(v_cur_o3):>10} {format_ms(v_best_o3):>10}  {r_o2:>7} {r_o3:>7}")

            row = {
                "op": op,
                "case": cf,
                "cur_o2_ms": v_cur_o2,
                "best_o2_ms": v_best_o2,
                "cur_o3_ms": v_cur_o3,
                "best_o3_ms": v_best_o3,
                "ratio_o2": r_o2,
                "ratio_o3": r_o3,
            }
            all_rows.append(row)

    # 写CSV
    if all_rows:
        os.makedirs(RESULTS_DIR, exist_ok=True)
        fields = ["op", "case", "cur_o2_ms", "best_o2_ms", "cur_o3_ms", "best_o3_ms", "ratio_o2", "ratio_o3"]
        with open(csv_path, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=fields)
            w.writeheader()
            w.writerows(all_rows)
        print(f"\n[bench] CSV saved: {csv_path}")

    # 汇总统计
    print(f"\n{'='*70}")
    print("=== SUMMARY (min ms, CUR/BEST ratio < 1.0 means CUR faster) ===")
    print(f"{'='*70}")
    print(f"{'op':<6} {'CUR_O2':>10} {'BEST_O2':>10} {'CUR_O3':>10} {'BEST_O3':>10}  {'C2/B2':>7} {'C3/B3':>7}")
    print("-" * 70)
    for op in ops_to_run:
        if op not in VERSIONS:
            continue
        rows = [r for r in all_rows if r["op"] == op]
        if not rows:
            continue
        def safe_min(key):
            vals = [r[key] for r in rows if isinstance(r[key], float)]
            return min(vals) if vals else 0
        s_co2 = safe_min("cur_o2_ms")
        s_bo2 = safe_min("best_o2_ms")
        s_co3 = safe_min("cur_o3_ms")
        s_bo3 = safe_min("best_o3_ms")
        r2 = f"{s_co2/s_bo2:.3f}" if s_bo2 > 0 else "-"
        r3 = f"{s_co3/s_bo3:.3f}" if s_bo3 > 0 else "-"
        print(f"{op:<6} {s_co2:>10.1f} {s_bo2:>10.1f} {s_co3:>10.1f} {s_bo3:>10.1f}  {r2:>7} {r3:>7}")


if __name__ == "__main__":
    main()
