#!/usr/bin/env python3
# VM benchmark via paramiko: upload -> compile 6 binaries -> gen data -> 5-run median -> report
import os
import re
import sys
import paramiko

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"
LOCAL_CPP = r"d:\precious_speed\moptm_fusion.cpp"
REMOTE_DIR = "/tmp/bench"
REMOTE_CPP = f"{REMOTE_DIR}/moptm_fusion.cpp"
LOCAL_REPORT = r"d:\precious_speed\VM_BENCH_20260720.md"
TODAY = "2026-07-20"

# Data generation script (runs on VM via python3)
GEN_DATA_SCRIPT = r'''
import os, random
os.makedirs("/tmp/bench", exist_ok=True)
random.seed(42)

def rand_digits(n, lo_first=1, hi_first=9):
    # first digit non-zero; rest 0-9
    parts = [str(random.randint(lo_first, hi_first))]
    parts.append(''.join(str(random.randint(0,9)) for _ in range(n-1)))
    return parts[0] + parts[1]

def write_case(path, a, b):
    with open(path, "w") as f:
        f.write("1\n")
        f.write(a + "\n")
        f.write(b + "\n")

# ADD: 1M + 1M (both first digit 1-9)
write_case("/tmp/bench/add_1M.in", rand_digits(1_000_000, 1, 9), rand_digits(1_000_000, 1, 9))
# MUL: 500k * 500k
write_case("/tmp/bench/mul_500k.in", rand_digits(500_000, 1, 9), rand_digits(500_000, 1, 9))
# DIV: 1M / 500k  (divisor leading digit 5-9 for normalization)
write_case("/tmp/bench/div_1M_500k.in", rand_digits(1_000_000, 1, 9), rand_digits(500_000, 5, 9))
# DIV: 200k / 100k
write_case("/tmp/bench/div_200k_100k.in", rand_digits(200_000, 1, 9), rand_digits(100_000, 5, 9))
print("data generated OK")
'''

COMPILE_TARGETS = [
    ("moptm_add",    "-O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD"),
    ("moptm_mul",    "-O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL"),
    ("moptm_div",    "-O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV"),
    ("moptm_add_o3", "-O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD"),
    ("moptm_mul_o3", "-O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL"),
    ("moptm_div_o3", "-O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV"),
]

# (label, input_file, O2_binary, O3_binary, min_expected_output_bytes)
BENCH_CASES = [
    ("ADD 1M+1M",       "add_1M.in",        "moptm_add",    "moptm_add_o3", 1_000_000),
    ("MUL 500k*500k",   "mul_500k.in",      "moptm_mul",    "moptm_mul_o3", 900_000),
    ("DIV 1M/500k",     "div_1M_500k.in",   "moptm_div",    "moptm_div_o3", 500_000),
    ("DIV 200k/100k",   "div_200k_100k.in", "moptm_div",    "moptm_div_o3", 100_000),
]


def get_client():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PWD, timeout=15)
    return c


def run(client, cmd, timeout=600):
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err


def upload(client, local, remote):
    sftp = client.open_sftp()
    try:
        sftp.put(local, remote)
    finally:
        sftp.close()


def download(client, remote, local):
    sftp = client.open_sftp()
    try:
        sftp.get(remote, local)
        return True
    except Exception as e:
        print(f"  [WARN] download {remote} -> {local} failed: {e}")
        return False
    finally:
        sftp.close()


def write_remote_file(client, remote_path, content):
    sftp = client.open_sftp()
    try:
        with sftp.open(remote_path, "w") as f:
            f.write(content)
    finally:
        sftp.close()


def extract_user_time_ms(time_v_output):
    m = re.search(r"User time \(seconds\):\s*([\d.]+)", time_v_output)
    if m:
        return float(m.group(1)) * 1000.0
    return None


def extract_max_rss_kb(time_v_output):
    m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", time_v_output)
    if m:
        return int(m.group(1))
    return None


def extract_exit_status(time_v_output):
    m = re.search(r"Exit status:\s*(-?\d+)", time_v_output)
    if m:
        return int(m.group(1))
    return None


def median(values):
    s = sorted(values)
    n = len(s)
    if n == 0:
        return None
    if n % 2 == 1:
        return s[n // 2]
    return (s[n // 2 - 1] + s[n // 2]) / 2


def main():
    print(f"[1/7] Connecting to {USER}@{HOST} ...")
    client = get_client()
    print("  [OK] connected")

    # basic info
    rc, distro, _ = run(client, "lsb_release -ds 2>/dev/null || (source /etc/os-release && echo \"$PRETTY_NAME\")")
    rc, gcc_ver, _ = run(client, "g++ --version | head -1")
    rc, cpu_model, _ = run(client, "lscpu 2>/dev/null | grep -E 'Model name' | head -1")
    rc, cpu_cores, _ = run(client, "nproc 2>/dev/null")
    rc, mem, _ = run(client, "free -h 2>/dev/null | awk '/^Mem:/ {print $2}'")
    distro_str = distro.strip() or "Linux"
    gcc_str = gcc_ver.strip() or "g++"
    cpu_str = cpu_model.strip().replace("Model name:", "").strip() or "unknown CPU"
    cores_str = cpu_cores.strip() or "?"
    mem_str = mem.strip() or "?"
    print(f"  VM: {distro_str} | {gcc_str} | {cpu_str} | {cores_str} cores | {mem_str}")

    print(f"[2/7] mkdir -p {REMOTE_DIR}")
    rc, out, err = run(client, f"mkdir -p {REMOTE_DIR}")
    print(f"  rc={rc} {err.strip() if err else ''}")

    print(f"[3/7] Upload {LOCAL_CPP} -> {REMOTE_CPP}")
    upload(client, LOCAL_CPP, REMOTE_CPP)
    rc, out, _ = run(client, f"ls -la {REMOTE_CPP} && wc -l {REMOTE_CPP}")
    print(f"  {out.strip()}")

    print(f"[4/7] Compile 6 binaries ...")
    for name, flags in COMPILE_TARGETS:
        cmd = f"cd {REMOTE_DIR} && g++ {flags} moptm_fusion.cpp -o {name} 2>&1"
        print(f"  -> {name}  ({flags})")
        rc, out, err = run(client, cmd, timeout=300)
        if rc != 0:
            print(f"     [FAIL rc={rc}]")
            print(out)
            print(err)
            client.close()
            sys.exit(1)
        # verify binary exists
        rc2, ls_out, _ = run(client, f"ls -la {REMOTE_DIR}/{name}")
        print(f"     [OK] {ls_out.strip().splitlines()[-1] if ls_out else ''}")

    print(f"[5/7] Check / generate test data ...")
    required_files = ["add_1M.in", "mul_500k.in", "div_1M_500k.in", "div_200k_100k.in"]
    rc, out, _ = run(client, f"ls {REMOTE_DIR}/*.in 2>/dev/null")
    existing = set()
    for line in out.splitlines():
        line = line.strip()
        if line:
            existing.add(os.path.basename(line))
    print(f"  existing .in files: {sorted(existing) if existing else '(none)'}")

    needs_regen = False
    missing = [f for f in required_files if f not in existing]
    if missing:
        print(f"  missing: {missing} -> will generate")
        needs_regen = True
    else:
        # verify format: first byte must be '1' (test count), and file size reasonable
        for fname in required_files:
            rc, head_out, _ = run(client, f"head -c 2 {REMOTE_DIR}/{fname} | od -c | head -1")
            rc, sz_out, _ = run(client, f"wc -c < {REMOTE_DIR}/{fname}")
            print(f"  {fname}: head={head_out.strip()!r} size={sz_out.strip()} bytes")
            if not head_out.strip().startswith("0000000   1"):
                print(f"    [WARN] {fname} first byte not '1', will regen")
                needs_regen = True
                break

    if needs_regen:
        print(f"  -> generating data on VM via python3 ...")
        script_path = f"{REMOTE_DIR}/gen_data.py"
        write_remote_file(client, script_path, GEN_DATA_SCRIPT)
        rc, out, err = run(client, f"python3 {script_path}", timeout=120)
        print(f"  rc={rc}")
        if out.strip():
            print(f"  stdout: {out.strip()}")
        if err.strip():
            print(f"  stderr: {err.strip()}")
        # verify
        rc, out, _ = run(client, f"ls -la {REMOTE_DIR}/*.in && echo '--- heads ---' && for f in {REMOTE_DIR}/*.in; do echo \"$f: $(head -c 1 $f) $(wc -c < $f)bytes\"; done")
        print(out)

    # IMPORTANT: /usr/bin/time -v 的 "User time (seconds)" 在 Linux 上以 clock_t 为单位,
    # 通常 100Hz = 10ms 粒度, 对 sub-10ms 操作完全失效 (显示 0.00).
    # 解决方案: 在 /usr/bin/time -v 内用 bash 循环跑 LOOPS 次, 总 User time / LOOPS
    # 得到等效单次 CPU 时间, 精度 = 10ms / LOOPS.
    LOOPS = 50  # 0.20 ms precision per single-run estimate
    print(f"[6/7] Run benchmarks (5 invocations, each loops {LOOPS}x inside /usr/bin/time -v) ...")
    results = []  # list of dicts
    for label, infile, bin_o2, bin_o3, min_size in BENCH_CASES:
        row = {"label": label, "infile": infile, "min_size": min_size,
               "loops": LOOPS,
               "o2": None, "o3": None, "o2_rss": None, "o3_rss": None,
               "o2_times": [], "o3_times": []}
        for opt_key, binname in [("o2", bin_o2), ("o3", bin_o3)]:
            times_ms = []
            rss_kb = None
            print(f"  -> {label} [{opt_key}] ({binname} < {infile}, {LOOPS} loops)")
            for i in range(5):
                # 在 /usr/bin/time -v 内循环 LOOPS 次, 然后 User time / LOOPS
                # 注意: ./bin < file 在每次循环里都重新 mmap stdin, 这是正确的
                inner = f"for i in $(seq 1 {LOOPS}); do ./{binname} < {infile} > /dev/null; done"
                cmd = (f"cd {REMOTE_DIR} && /usr/bin/time -v bash -c '{inner}' "
                       f"> /dev/null 2> /tmp/time_tmp_{binname}_{i}.txt; "
                       f"echo '--- BEGIN TIME ---'; "
                       f"grep -E 'User time|Maximum resident|Exit status|Elapsed' /tmp/time_tmp_{binname}_{i}.txt; "
                       f"echo '--- END TIME ---'")
                rc, out, err = run(client, cmd, timeout=900)
                t_total = extract_user_time_ms(out)
                es = extract_exit_status(out)
                rss = extract_max_rss_kb(out)
                if rss is not None:
                    rss_kb = rss
                if t_total is None:
                    print(f"    [run {i+1}] FAIL extract time (rc={rc}, exit_status={es})")
                    print(out)
                    if err.strip():
                        print(f"    [ssh stderr] {err.strip()}")
                    continue
                t_per_run = t_total / LOOPS
                times_ms.append(t_per_run)
                print(f"    [run {i+1}] total={t_total:8.2f}ms  per_run={t_per_run:7.3f}ms  (exit={es}, max_rss={rss} kB)")
            row[f"{opt_key}_times"] = times_ms
            row[opt_key] = median(times_ms)
            row[f"{opt_key}_rss"] = rss_kb
        results.append(row)

    print(f"\n[7/7] Correctness verification (single run, check output size) ...")
    for label, infile, bin_o2, bin_o3, min_size in BENCH_CASES:
        cmd = f"cd {REMOTE_DIR} && ./{bin_o2} < {infile} | wc -c"
        rc, out, err = run(client, cmd, timeout=300)
        try:
            sz = int(out.strip())
        except ValueError:
            sz = -1
        ok = sz >= min_size
        print(f"  {label:18s} ({bin_o2}): output={sz} bytes  {'[OK]' if ok else '[WARN unexpected size]'}")

    # download prof_result.txt if exists
    print("\nDownloading prof_result.txt if exists ...")
    rc, out, _ = run(client, f"ls -la {REMOTE_DIR}/prof_result.txt 2>/dev/null")
    if "prof_result.txt" in out:
        ok = download(client, f"{REMOTE_DIR}/prof_result.txt", r"d:\precious_speed\prof_result.txt")
        if ok:
            print("  [OK] downloaded prof_result.txt")
    else:
        print("  not present (PROFILE_DIV not defined at compile)")

    # also persist raw timing log
    rc, out, _ = run(client, f"cd {REMOTE_DIR} && cat time_tmp_*.txt 2>/dev/null | head -200")
    raw_log = out
    with open(r"d:\precious_speed\vm_bench_raw_times.log", "w", encoding="utf-8") as f:
        f.write(f"=== VM bench raw time logs ({TODAY}) ===\n")
        f.write(raw_log)
        f.write("\n=== Per-run summary (ms) ===\n")
        for row in results:
            f.write(f"{row['label']} [{row['infile']}]\n")
            f.write(f"  O2 times: {row['o2_times']}  -> median={row['o2']}\n")
            f.write(f"  O3 times: {row['o3_times']}  -> median={row['o3']}\n")
            f.write(f"  O2 max_rss={row['o2_rss']} kB   O3 max_rss={row['o3_rss']} kB\n")
    print("  saved raw times -> d:\\precious_speed\\vm_bench_raw_times.log")

    # write report markdown
    print(f"\nWriting report -> {LOCAL_REPORT}")
    lines = []
    lines.append(f"# VM Benchmark {TODAY}")
    lines.append("")
    lines.append("## 环境")
    lines.append(f"- VM: `{HOST}` ({distro_str})")
    lines.append(f"- Compiler: `{gcc_str}`")
    lines.append(f"- CPU: {cpu_str}  ({cores_str} cores)")
    lines.append(f"- Memory: {mem_str}")
    lines.append("- 编译: `-O2/-O3 -std=gnu++20 -static -DONLINE_JUDGE`")
    lines.append("- 源文件: `moptm_fusion.cpp` (moptm v2 融合优化最终版)")
    lines.append("- 数据: ADD=1M+1M, MUL=500k*500k, DIV=1M/500k, DIV=200k/100k")
    lines.append("- 测量方法: 5 次 `/usr/bin/time -v` 调用, 取中位数; 每次调用内用 bash 循环跑 LOOPS=50 次, 取 `User time / 50` 为单次等效 CPU 时间")
    lines.append("- 上述方法解决 `/usr/bin/time -v` 的 10ms 粒度问题 (clock_t=100Hz), 等效精度 = 10ms / 50 = 0.2ms")
    lines.append("- DIV 同一二进制 (`moptm_div` / `moptm_div_o3`) 跑两个 DIV 用例")
    lines.append("")
    lines.append("## 结果 (5 次中位数, ms)")
    lines.append("")
    lines.append("| 测试 | O2 | O3 | O2 max RSS (MB) | O3 max RSS (MB) |")
    lines.append("|------|-----|-----|-----|-----|")
    for row in results:
        o2 = f"{row['o2']:.2f}" if row['o2'] is not None else "N/A"
        o3 = f"{row['o3']:.2f}" if row['o3'] is not None else "N/A"
        o2_rss = f"{row['o2_rss']/1024:.1f}" if row['o2_rss'] else "-"
        o3_rss = f"{row['o3_rss']/1024:.1f}" if row['o3_rss'] else "-"
        lines.append(f"| {row['label']} | {o2} | {o3} | {o2_rss} | {o3_rss} |")
    lines.append("")
    lines.append("## 详细 (每次运行, ms)")
    lines.append("")
    lines.append("| 测试 | run1 O2 | run2 O2 | run3 O2 | run4 O2 | run5 O2 | run1 O3 | run2 O3 | run3 O3 | run4 O3 | run5 O3 |")
    lines.append("|------|---|---|---|---|---|---|---|---|---|---|")
    for row in results:
        o2_runs = row['o2_times'] + [None] * (5 - len(row['o2_times']))
        o3_runs = row['o3_times'] + [None] * (5 - len(row['o3_times']))
        cells = []
        for v in o2_runs:
            cells.append(f"{v:.2f}" if v is not None else "-")
        for v in o3_runs:
            cells.append(f"{v:.2f}" if v is not None else "-")
        lines.append(f"| {row['label']} | " + " | ".join(cells) + " |")
    lines.append("")
    lines.append("## 说明")
    lines.append("- User time = 用户态 CPU 时间 (不含内核态/IO等待)")
    lines.append("- 程序使用 mmap 零拷贝读 stdin, 必须用文件重定向 `< file`")
    lines.append("- 5 次中位数 = 排序后第 3 个值")
    lines.append("- 正确性已通过 wc -c 输出字节数粗验")
    lines.append("- **粒度问题**: `/usr/bin/time -v` 的 User time 以 clock_t (100Hz=10ms) 为单位, 对 sub-10ms 操作显示 0.00; 本测试在 `/usr/bin/time -v` 内 bash 循环 LOOPS=50 次, 取 `User time / 50` 为单次等效 CPU 时间 (精度 0.2ms), 该值为含二进制启动+I/O 的总单次时间")
    lines.append("")

    report = "\n".join(lines)
    with open(LOCAL_REPORT, "w", encoding="utf-8") as f:
        f.write(report)
    print()
    print(report)

    client.close()
    print("\n=== DONE ===")


if __name__ == "__main__":
    main()
