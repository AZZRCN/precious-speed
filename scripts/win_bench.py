"""
win_bench.py - Windows 本地 O2/O3 三版本对比 benchmark
测试 fusion_o2only / fusion(v2,pragma O3) / moptm / best 在 O2/O3 下三题性能
"""
import os
import subprocess
import time
import statistics
import sys

WORK = r"d:\precious_speed\winbench"
os.makedirs(WORK, exist_ok=True)

SOURCES = {
    "fusion_o2only": r"d:\precious_speed\fusion_o2only.cpp",
    "fusion": r"d:\precious_speed\fusion.cpp",
    "moptm": r"d:\precious_speed\archieve\cpp\moptm.cpp",
    "best_add": r"d:\precious_speed\best\add.cpp",
    "best_mul": r"d:\precious_speed\best\mul.cpp",
    "best_div": r"d:\precious_speed\best\div.cpp",
}

OPS = ["ADD", "MUL", "DIV"]
OPTS = ["O2", "O3"]


def gen_data():
    """生成测试数据"""
    import random
    random.seed(42)
    # ADD 1M+1M
    if not os.path.exists(os.path.join(WORK, "add_1M.in")):
        a = "".join(random.choices("0123456789", k=1000000))
        b = "".join(random.choices("0123456789", k=1000000))
        with open(os.path.join(WORK, "add_1M.in"), "w") as f:
            f.write(f"{a}\n{b}\n")
    # MUL 500k*500k
    if not os.path.exists(os.path.join(WORK, "mul_500k.in")):
        a = "".join(random.choices("0123456789", k=500000))
        b = "".join(random.choices("0123456789", k=500000))
        with open(os.path.join(WORK, "mul_500k.in"), "w") as f:
            f.write(f"{a}\n{b}\n")
    # DIV 1M/500k
    if not os.path.exists(os.path.join(WORK, "div_1M_500k.in")):
        a = "".join(random.choices("0123456789", k=1000000))
        b = "".join(random.choices("0123456789", k=500000))
        with open(os.path.join(WORK, "div_1M_500k.in"), "w") as f:
            f.write(f"{a}\n{b}\n")
    print("[OK] test data ready")


def compile_all():
    """编译所有组合，返回 {binary_name: exe_path}"""
    binaries = {}
    for src_name, src_path in SOURCES.items():
        for opt in OPTS:
            for op in OPS:
                # best 只有一个 main，不需要 -D
                if src_name.startswith("best_"):
                    if not src_name.endswith(op.lower()):
                        continue
                    extra = []
                else:
                    extra = [f"-DHINT_OP_{op}"]
                bin_name = f"{src_name}_{opt}_{op}"
                exe = os.path.join(WORK, f"{bin_name}.exe")
                cmd = ["g++", f"-{opt}", "-std=gnu++20", "-DONLINE_JUDGE"] + extra + [src_path, "-o", exe]
                print(f"[COMPILE] {bin_name}...", end=" ", flush=True)
                r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
                if r.returncode != 0:
                    print(f"FAIL")
                    print(r.stderr[:500])
                    continue
                binaries[bin_name] = exe
                print("OK")
    return binaries


def run_bench(exe, infile, runs=5, warmup=2):
    """跑 benchmark，返回中位数（毫秒）"""
    times = []
    # warmup
    for _ in range(warmup):
        with open(infile, "rb") as f:
            subprocess.run([exe], stdin=f, capture_output=True, timeout=30)
    # real runs
    for _ in range(runs):
        with open(infile, "rb") as f:
            t0 = time.perf_counter()
            r = subprocess.run([exe], stdin=f, capture_output=True, timeout=30)
            t1 = time.perf_counter()
            if not r.stdout:
                return None
            times.append((t1 - t0) * 1000)
    return statistics.median(times)


def main():
    print("=== Windows Local Benchmark (O2 vs O3) ===")
    print(f"Date: {time.strftime('%Y-%m-%d %H:%M:%S')}")
    r = subprocess.run(["g++", "--version"], capture_output=True, text=True)
    print(f"g++: {r.stdout.splitlines()[0]}")
    print()

    gen_data()
    binaries = compile_all()
    print()

    inputs = {
        "ADD": os.path.join(WORK, "add_1M.in"),
        "MUL": os.path.join(WORK, "mul_500k.in"),
        "DIV": os.path.join(WORK, "div_1M_500k.in"),
    }

    print("=== Benchmark Results (median of 5 runs, ms) ===")
    print(f"{'Binary':<28}{'ADD_1M':<12}{'MUL_500k':<12}{'DIV_1M_500k':<12}")
    print("-" * 64)

    results = {}
    # 按版本分组
    groups = ["fusion_o2only", "fusion", "moptm", "best"]
    for group in groups:
        for opt in OPTS:
            row = []
            for op in OPS:
                bin_name = f"{group}_{opt}_{op}" if group != "best" else f"best_{op.lower()}_{opt}_{op}"
                # 实际命名：best_add_O2_ADD
                if group == "best":
                    bin_name = f"best_{op.lower()}_{opt}_{op}"
                exe = binaries.get(bin_name)
                if not exe:
                    row.append("N/A")
                    continue
                t = run_bench(exe, inputs[op])
                if t is None:
                    row.append("FAIL")
                    continue
                row.append(f"{t:.2f}")
                results[bin_name] = t
            label = f"{group}_{opt}" if group != "best" else f"best_{opt}"
            print(f"{label:<28}{row[0]:<12}{row[1]:<12}{row[2]:<12}")
            sys.stdout.flush()

    print()
    print("=== Done ===")

    # 输出 CSV
    with open(os.path.join(WORK, "results.csv"), "w") as f:
        f.write("binary,ADD_1M,MUL_500k,DIV_1M_500k\n")
        for group in groups:
            for opt in OPTS:
                row = []
                for op in OPS:
                    if group == "best":
                        bin_name = f"best_{op.lower()}_{opt}_{op}"
                    else:
                        bin_name = f"{group}_{opt}_{op}"
                    t = results.get(bin_name)
                    row.append(f"{t:.3f}" if t else "N/A")
                label = f"{group}_{opt}" if group != "best" else f"best_{opt}"
                f.write(f"{label},{row[0]},{row[1]},{row[2]}\n")
    print(f"Results saved to {WORK}\\results.csv")


if __name__ == "__main__":
    main()
