#!/usr/bin/env python3
"""
统一测速制度: 本地 (Windows/MinGW) + 远程 (SSH/Ubuntu VM)
口径: N 次取最小总时间 (ms)
编译统一走 ai.bat (compile_div/add/mul), 与 LC 环境 (-O2 -march=native) 对齐
用法:
  python bench.py --local --op all
  python bench.py --remote --op div
  python bench.py --both --op add,mul,div --iters 5 --tag baseline
结果: bench/results/bench_<timestamp>.csv
"""
import argparse, subprocess, time, sys, os, csv, datetime, statistics

ROOT = os.path.dirname(os.path.abspath(__file__))
PARENT = os.path.dirname(ROOT)
BENCH_DATA = os.path.join(PARENT, "bench_data")
RESULTS_DIR = os.path.join(ROOT, "results")
AI_BAT = os.path.join(PARENT, "ai.bat")
os.makedirs(RESULTS_DIR, exist_ok=True)

# 测速配置: op -> [(case_file, label)]
CASES = {
    "add": [
        ("add_medium_0.in", "med0"),
        ("add_medium_1.in", "med1"),
        ("add_large_0.in", "large"),
        ("add_max_0.in",   "max0"),
        ("add_max_1.in",   "max1"),
    ],
    "mul": [
        ("mul_medium_0.in", "med"),
        ("mul_large_0.in",  "large"),
        ("mul_max_0.in",    "max0"),
        ("mul_max_1.in",    "max1"),
    ],
    "div": [
        ("div_medium_0.in", "med"),
        ("div_large_0.in",  "large"),
        ("div_max_0.in",    "max0"),
        ("div_max_2.in",    "max2"),
    ],
}

# 本地 exe (由 ai.bat 生成)
LOCAL_EXE = {
    "add": os.path.join(PARENT, "cur_add.exe"),
    "mul": os.path.join(PARENT, "cur_mul.exe"),
    "div": os.path.join(PARENT, "cur_div.exe"),
}

# 远程编译/运行路径 (LC Ubuntu 对齐: -O2, 不加 -march=native 因 VM 是 AMD Zen3)
REMOTE_CXX = "g++"
REMOTE_CXXFLAGS = ["-std=gnu++20", "-O2", "-DONLINE_JUDGE"]
REMOTE_LDFLAGS = ["-pthread"]


def compile_local_via_aibat(op):
    """通过 ai.bat 编译, 30s 超时, 返回 (rc, stderr)"""
    step = f"compile_{op}"
    r = subprocess.run([AI_BAT, step], capture_output=True, text=True, timeout=60)
    return r.returncode, r.stdout + r.stderr


def run_local_bench(exe, case_file, iters=5, timeout=60):
    """本地测速: N 次取最小总时间 (ms)"""
    inp = os.path.join(BENCH_DATA, case_file)
    if not os.path.exists(inp):
        return {"rc": -1, "err": f"input not found: {inp}"}
    if not os.path.exists(exe):
        return {"rc": -1, "err": f"exe not found: {exe}"}
    times = []
    for _ in range(iters):
        t0 = time.perf_counter()
        try:
            with open(inp, "rb") as fin:
                r = subprocess.run([exe], stdin=fin,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                   timeout=timeout)
            t1 = time.perf_counter()
            if r.returncode == 0:
                times.append((t1 - t0) * 1000)
            else:
                return {"rc": r.returncode, "err": r.stderr.decode(errors="replace")[:200]}
        except subprocess.TimeoutExpired:
            return {"rc": -2, "err": "timeout"}
    return {"rc": 0, "min_ms": min(times), "med_ms": statistics.median(times),
            "all_ms": ",".join(f"{t:.1f}" for t in times)}


def run_remote_bench(ssh, exe_remote, case_file, iters=5, timeout=120):
    """远程测速: 上传输入, N 次取最小 (用 /usr/bin/time)"""
    inp_local = os.path.join(BENCH_DATA, case_file)
    if not os.path.exists(inp_local):
        return {"rc": -1, "err": f"input not found: {inp_local}"}
    inp_remote = f"/tmp/bench_{case_file}"
    ssh.upload(inp_local, inp_remote)
    script = f"""best=999999
for i in $(seq 1 {iters}); do
    t=$(/usr/bin/time -f '%e' {exe_remote} < {inp_remote} 2>&1 >/dev/null)
    rc=$?
    if [ $rc -ne 0 ]; then echo "FAIL:$rc"; exit $rc; fi
    python3 -c "exit(0 if float('$t') < $best else 1)" 2>/dev/null && best=$t || true
done
echo $best
"""
    rc, out, err = ssh.run(script, timeout=timeout * iters + 30)
    out = out.strip()
    if rc != 0 or out.startswith("FAIL:"):
        return {"rc": rc, "err": out + err}
    try:
        best_s = float(out.splitlines()[-1])
        return {"rc": 0, "min_ms": best_s * 1000, "all_ms": ""}
    except Exception:
        return {"rc": -3, "err": f"parse fail: {out}"}


def compile_remote(ssh, op, src_local, out_remote):
    """远程编译, 返回 (rc, stderr)"""
    ssh.upload(src_local, out_remote + ".cpp")
    flags = REMOTE_CXXFLAGS + [f"-DHINT_OP_{op.upper()}"]
    cmd = f"{REMOTE_CXX} {' '.join(flags)} {out_remote}.cpp -o {out_remote} {' '.join(REMOTE_LDFLAGS)} 2>&1"
    rc, out, err = ssh.run(cmd, timeout=120)
    return rc, out + err


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--local", action="store_true")
    ap.add_argument("--remote", action="store_true")
    ap.add_argument("--both", action="store_true")
    ap.add_argument("--op", default="div", help="add|mul|div|all (逗号分隔)")
    ap.add_argument("--cases", default="all", help="all 或 label 逗号分隔")
    ap.add_argument("--iters", type=int, default=5)
    ap.add_argument("--no-compile", action="store_true", help="跳过编译, 直接测速现有 exe")
    ap.add_argument("--tag", default="", help="结果标签 (如 baseline, cyclic)")
    args = ap.parse_args()

    if args.both:
        args.local = args.remote = True
    if not (args.local or args.remote):
        args.local = True

    ops = ["add", "mul", "div"] if args.op == "all" else args.op.split(",")

    ssh = None
    if args.remote:
        sys.path.insert(0, PARENT)
        from ssh_manager import get_ssh
        ssh = get_ssh()

    rows = []
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    tag = args.tag or "default"

    for op in ops:
        src = os.path.join(PARENT, f"{op}.cpp")
        if not os.path.exists(src):
            print(f"[skip] {op}: source not found {src}")
            continue

        case_list = CASES.get(op, [])
        if args.cases != "all":
            wanted = set(args.cases.split(","))
            case_list = [c for c in case_list if c[1] in wanted]

        # 本地测速
        if args.local:
            print(f"\n=== LOCAL {op} ({src}) ===")
            exe_local = LOCAL_EXE[op]
            if not args.no_compile:
                rc, err = compile_local_via_aibat(op)
                if rc != 0:
                    print(f"  compile FAIL (rc={rc}): {err[:300]}")
                    continue
                print(f"  compiled OK via ai.bat")
            else:
                print(f"  skip compile (use existing {exe_local})")
            for cf, label in case_list:
                r = run_local_bench(exe_local, cf, args.iters)
                if r["rc"] == 0:
                    print(f"  {cf:<20} [{label:<5}]: min={r['min_ms']:7.1f}ms med={r['med_ms']:7.1f}ms  [{r['all_ms']}]")
                    rows.append({"ts": ts, "host": "local", "op": op, "case": cf, "label": label,
                                 "min_ms": f"{r['min_ms']:.1f}", "med_ms": f"{r['med_ms']:.1f}",
                                 "all_ms": r["all_ms"], "tag": tag})
                else:
                    print(f"  {cf:<20} [{label:<5}]: FAIL rc={r['rc']} {r.get('err','')[:100]}")

        # 远程测速
        if args.remote and ssh is not None:
            print(f"\n=== REMOTE {op} ({src}) ===")
            exe_remote = f"/home/azzr/bench_{op}"
            rc, err = compile_remote(ssh, op, src, exe_remote)
            if rc != 0:
                print(f"  compile FAIL: {err[:300]}")
                continue
            print(f"  compiled OK")
            for cf, label in case_list:
                r = run_remote_bench(ssh, exe_remote, cf, args.iters)
                if r["rc"] == 0:
                    print(f"  {cf:<20} [{label:<5}]: min={r['min_ms']:7.1f}ms")
                    rows.append({"ts": ts, "host": ssh.host, "op": op, "case": cf, "label": label,
                                 "min_ms": f"{r['min_ms']:.1f}", "med_ms": "",
                                 "all_ms": r.get("all_ms", ""), "tag": tag})
                else:
                    print(f"  {cf:<20} [{label:<5}]: FAIL rc={r['rc']} {r.get('err','')[:100]}")

    # 写 CSV
    if rows:
        csv_path = os.path.join(RESULTS_DIR, f"bench_{ts}.csv")
        with open(csv_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["ts", "host", "op", "case", "label", "min_ms", "med_ms", "all_ms", "tag"])
            w.writeheader()
            w.writerows(rows)
        print(f"\n=== Results saved: {csv_path} ({len(rows)} rows) ===")


if __name__ == "__main__":
    main()
