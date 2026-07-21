#!/usr/bin/env python3
"""
VM benchmark for moptm_fusion.cpp O2/O3.
- Uploads source + test data via SFTP
- Generates div_200k_100k.in on VM if missing (not in winbench/)
- Compiles 6 binaries (O2/O3 x ADD/MUL/DIV)
- Runs 5 invocations of bash builtin `time` (TIMEFORMAT=%U) on a 50-loop bash loop
- per_run = total_user_seconds * 1000 / 50
- Reports median per_run, max RSS, correctness (wc -c)
"""
import os
import re
import sys
import time
import paramiko

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"
REMOTE_DIR = "/tmp/bench"
LOCAL_CPP = r"d:\precious_speed\moptm_fusion.cpp"
LOCAL_WINBENCH = r"d:\precious_speed\winbench"
LOOPS = 50
RUNS = 5

# (label, input_file, O2_binary, O3_binary, min_expected_output_bytes,
#  prev_O2_ms, prev_O3_ms)  -- prev_* from VM_BENCH_20260720.md
BENCH_CASES = [
    ("ADD 1M+1M",       "add_1M.in",        "moptm_add",    "moptm_add_o3", 1_000_000, 0.620, 0.460),
    ("MUL 500k*500k",   "mul_500k.in",      "moptm_mul",    "moptm_mul_o3", 900_000,   4.300, 4.280),
    ("DIV 1M/500k",     "div_1M_500k.in",   "moptm_div",    "moptm_div_o3", 500_000,  16.640, 16.000),
    ("DIV 200k/100k",   "div_200k_100k.in", "moptm_div",    "moptm_div_o3", 100_000,   3.520, 4.380),
]

COMPILE_TARGETS = [
    ("moptm_add",    "-O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD"),
    ("moptm_mul",    "-O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL"),
    ("moptm_div",    "-O2 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV"),
    ("moptm_add_o3", "-O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_ADD"),
    ("moptm_mul_o3", "-O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_MUL"),
    ("moptm_div_o3", "-O3 -std=gnu++20 -static -DONLINE_JUDGE -DHINT_OP_DIV"),
]

# Script to generate div_200k_100k.in on VM via python3 (same format as vm_bench_paramiko.py)
GEN_DIV_200K_SCRIPT = r'''
import random
random.seed(42)
def rand_digits(n, lo_first=1, hi_first=9):
    parts = [str(random.randint(lo_first, hi_first))]
    parts.append(''.join(str(random.randint(0,9)) for _ in range(n-1)))
    return parts[0] + parts[1]
# DIV 200k / 100k (divisor leading digit 5-9 for normalization)
with open("/tmp/bench/div_200k_100k.in", "w") as f:
    f.write("1\n")
    f.write(rand_digits(200_000, 1, 9) + "\n")
    f.write(rand_digits(100_000, 5, 9) + "\n")
print("div_200k_100k.in generated")
'''


def get_client():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PWD, timeout=15)
    return c


def run(client, cmd, timeout=900):
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err


def upload(client, local, remote):
    sftp = client.open_sftp()
    try:
        sftp.put(local, remote)
        print(f"  uploaded {local} -> {remote}")
    finally:
        sftp.close()


def write_remote_file(client, remote_path, content):
    sftp = client.open_sftp()
    try:
        with sftp.open(remote_path, "w") as f:
            f.write(content)
    finally:
        sftp.close()


def median(values):
    s = sorted(values)
    n = len(s)
    if n == 0:
        return None
    if n % 2 == 1:
        return s[n // 2]
    return (s[n // 2 - 1] + s[n // 2]) / 2


def measure_via_bash_time(client, binary, infile, loops=LOOPS, runs=RUNS):
    """Run `bash -c 'TIMEFORMAT="%U"; time { for i in $(seq 1 N); do ./bin < in > /dev/null; done; }'` 2>&1
    Parse the %U (user CPU seconds) on the last line.
    Returns (per_run_ms_list, raw_total_user_seconds_list)."""
    per_run_ms_list = []
    raw_totals_s = []
    for i in range(runs):
        cmd = (
            f"cd {REMOTE_DIR} && "
            f"bash -c 'TIMEFORMAT=\"%U\"; "
            f"time {{ for i in $(seq 1 {loops}); do ./{binary} < {infile} > /dev/null; done; }}' 2>&1"
        )
        rc, out, err = run(client, cmd, timeout=900)
        lines = [l.strip() for l in out.splitlines() if l.strip()]
        if not lines:
            print(f"    [run {i+1}] EMPTY output, rc={rc}, err={err.strip()[:200]}")
            continue
        try:
            total_user_s = float(lines[-1])
        except ValueError:
            print(f"    [run {i+1}] FAIL parse last line={lines[-1]!r}, full out={out!r}")
            continue
        per_run_ms = (total_user_s * 1000.0) / loops
        per_run_ms_list.append(per_run_ms)
        raw_totals_s.append(total_user_s)
        print(f"    [run {i+1}] loops_total_user={total_user_s*1000:7.2f}ms  per_run={per_run_ms:7.4f}ms")
    return per_run_ms_list, raw_totals_s


def measure_max_rss(client, binary, infile):
    """Single /usr/bin/time -v run just to get max RSS."""
    cmd = (f"cd {REMOTE_DIR} && /usr/bin/time -v ./{binary} < {infile} > /dev/null 2>&1 "
           f"| grep -E 'Maximum resident|Exit status'")
    rc, out, _ = run(client, cmd, timeout=300)
    m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", out)
    es = re.search(r"Exit status:\s*(-?\d+)", out)
    rss = int(m.group(1)) if m else None
    exit_code = int(es.group(1)) if es else None
    return rss, exit_code


def main():
    print(f"[1/6] Connecting to {USER}@{HOST} ...")
    client = get_client()
    print("  [OK] connected")

    # Env info
    print(f"[2/6] Gathering environment info ...")
    rc, cpu_info, _ = run(client, "grep -m1 'model name' /proc/cpuinfo")
    m = re.search(r":\s*(.+)", cpu_info)
    cpu_model = m.group(1).strip() if m else "unknown"
    rc, cpu_mhz, _ = run(client, "grep -m1 'cpu MHz' /proc/cpuinfo")
    m = re.search(r":\s*([\d.]+)", cpu_mhz)
    cpu_mhz_val = float(m.group(1)) if m else 0
    rc, cpu_flags, _ = run(client, "grep -m1 flags /proc/cpuinfo")
    has_avx2 = " avx2 " in cpu_flags if cpu_flags else False
    has_avx512 = " avx512f " in cpu_flags if cpu_flags else False
    rc, cores, _ = run(client, "nproc")
    rc, mem_total, _ = run(client, "grep MemTotal /proc/meminfo")
    m = re.search(r":\s+(\d+)", mem_total)
    mem_kb = int(m.group(1)) if m else 0
    mem_mib = mem_kb / 1024
    rc, distro, _ = run(client, "grep PRETTY_NAME /etc/os-release")
    m = re.search(r"=\"([^\"]+)\"", distro)
    distro_str = m.group(1) if m else "Linux"
    rc, kernel, _ = run(client, "uname -r")
    kernel_str = kernel.strip()
    rc, gcc_ver, _ = run(client, "g++ --version | head -1")
    gcc_str = gcc_ver.strip()
    rc, clk_tck, _ = run(client, "getconf CLK_TCK")
    clk_tck_str = clk_tck.strip()
    print(f"  CPU:    {cpu_model} @ {cpu_mhz_val:.0f} MHz")
    print(f"  Cores:  {cores.strip()}  (AVX2: {has_avx2}, AVX-512: {has_avx512})")
    print(f"  Mem:    {mem_mib:.0f} MiB")
    print(f"  OS:     {distro_str} (kernel {kernel_str})")
    print(f"  g++:    {gcc_str}")
    print(f"  CLK_TCK={clk_tck_str}")

    # mkdir
    print(f"\n[3/6] mkdir -p {REMOTE_DIR}")
    rc, out, err = run(client, f"mkdir -p {REMOTE_DIR}")
    print(f"  rc={rc} {err.strip() if err else ''}")

    # Upload source
    print(f"\n[4/6] Upload moptm_fusion.cpp ...")
    upload(client, LOCAL_CPP, f"{REMOTE_DIR}/moptm_fusion.cpp")
    rc, out, _ = run(client, f"ls -la {REMOTE_DIR}/moptm_fusion.cpp && wc -l {REMOTE_DIR}/moptm_fusion.cpp")
    print(f"  {out.strip()}")

    # Check & upload test data
    print(f"\n  Checking test data on VM ...")
    rc, out, _ = run(client, f"ls {REMOTE_DIR}/*.in 2>/dev/null")
    existing = set()
    for line in out.splitlines():
        line = line.strip()
        if line:
            existing.add(os.path.basename(line))
    print(f"  existing .in files on VM: {sorted(existing) if existing else '(none)'}")

    required = ["add_1M.in", "mul_500k.in", "div_1M_500k.in", "div_200k_100k.in"]
    for fname in required:
        local_path = os.path.join(LOCAL_WINBENCH, fname)
        remote_path = f"{REMOTE_DIR}/{fname}"
        if fname in existing:
            # verify size > 0
            rc, sz_out, _ = run(client, f"wc -c < {remote_path}")
            sz = sz_out.strip()
            print(f"  {fname}: already on VM ({sz} bytes), skip upload")
            continue
        if os.path.exists(local_path):
            upload(client, local_path, remote_path)
            rc, sz_out, _ = run(client, f"wc -c < {remote_path}")
            print(f"    VM size: {sz_out.strip()} bytes")
        else:
            # div_200k_100k.in is not in winbench/ -> generate on VM
            print(f"  {fname}: not in winbench/ ({local_path} missing) -> will generate on VM")
            script_path = f"{REMOTE_DIR}/gen_div_200k.py"
            write_remote_file(client, script_path, GEN_DIV_200K_SCRIPT)
            rc, out, err = run(client, f"python3 {script_path}", timeout=60)
            print(f"    rc={rc}  stdout: {out.strip()}")
            if err.strip():
                print(f"    stderr: {err.strip()}")
            rc, sz_out, _ = run(client, f"wc -c < {remote_path}")
            print(f"    VM size: {sz_out.strip()} bytes")

    # Final verify all 4 .in files
    print(f"\n  Final .in inventory on VM:")
    rc, out, _ = run(client, f"ls -la {REMOTE_DIR}/*.in")
    print(out)

    # Compile
    print(f"\n[5/6] Compile 6 binaries ...")
    for name, flags in COMPILE_TARGETS:
        cmd = f"cd {REMOTE_DIR} && g++ {flags} moptm_fusion.cpp -o {name} 2>&1"
        print(f"  -> {name}  ({flags})")
        t0 = time.time()
        rc, out, err = run(client, cmd, timeout=300)
        dt = time.time() - t0
        if rc != 0:
            print(f"     [FAIL rc={rc} in {dt:.1f}s]")
            print(out)
            print(err)
            client.close()
            sys.exit(1)
        rc2, ls_out, _ = run(client, f"ls -la {REMOTE_DIR}/{name} | awk '{{print $5, $9}}'")
        print(f"     [OK in {dt:.1f}s] size={ls_out.strip()}")

    # Benchmarks
    print(f"\n[6/6] Benchmarks (bash builtin time, {LOOPS} loops/run, {RUNS} runs each) ...")
    results = []
    for label, infile, bin_o2, bin_o3, min_size, prev_o2, prev_o3 in BENCH_CASES:
        row = {"label": label, "infile": infile, "loops": LOOPS,
               "o2": None, "o3": None, "o2_rss": None, "o3_rss": None,
               "o2_times": [], "o3_times": [], "o2_totals": [], "o3_totals": [],
               "prev_o2": prev_o2, "prev_o3": prev_o3}
        for opt_key, binname in [("o2", bin_o2), ("o3", bin_o3)]:
            print(f"  -> {label} [{opt_key}] ({binname} < {infile}, {LOOPS} loops)")
            per_run_list, total_list = measure_via_bash_time(client, binname, infile, LOOPS, RUNS)
            row[f"{opt_key}_times"] = per_run_list
            row[f"{opt_key}_totals"] = total_list
            row[opt_key] = median(per_run_list)
            rss, es = measure_max_rss(client, binname, infile)
            row[f"{opt_key}_rss"] = rss
            med_str = f"{row[opt_key]:.4f}ms" if row[opt_key] is not None else "N/A"
            print(f"     median_per_run={med_str}  max_rss={rss} kB (exit={es})")
        results.append(row)

    # Correctness
    print(f"\n  Correctness (single run, wc -c) ...")
    for label, infile, bin_o2, bin_o3, min_size, prev_o2, prev_o3 in BENCH_CASES:
        cmd = f"cd {REMOTE_DIR} && ./{bin_o2} < {infile} | wc -c"
        rc, out, _ = run(client, cmd, timeout=300)
        try:
            sz = int(out.strip())
        except ValueError:
            sz = -1
        ok = sz >= min_size
        row_ok = "[OK]" if ok else "[WARN unexpected size]"
        print(f"    {label:18s} ({bin_o2}): output={sz} bytes  {row_ok} (min={min_size})")

    # Results table
    print("\n" + "=" * 80)
    print("RESULTS (median per_run, ms)")
    print("=" * 80)
    print(f"{'测试':<18s} | {'O2 (ms)':>10s} | {'O3 (ms)':>10s} | {'O2/O3':>7s} | {'O2 RSS (MB)':>12s} | {'O3 RSS (MB)':>12s}")
    print("-" * 80)
    for row in results:
        o2 = f"{row['o2']:.3f}" if row['o2'] is not None else "N/A"
        o3 = f"{row['o3']:.3f}" if row['o3'] is not None else "N/A"
        if row['o2'] and row['o3']:
            ratio = f"{row['o2']/row['o3']:.3f}"
        else:
            ratio = "-"
        o2_rss = f"{row['o2_rss']/1024:.1f}" if row['o2_rss'] else "-"
        o3_rss = f"{row['o3_rss']/1024:.1f}" if row['o3_rss'] else "-"
        print(f"{row['label']:<18s} | {o2:>10s} | {o3:>10s} | {ratio:>7s} | {o2_rss:>12s} | {o3_rss:>12s}")

    print("\nPER-RUN DETAIL (ms)")
    print("-" * 80)
    print(f"{'测试':<18s} | {'r1 O2':>7s} {'r2 O2':>7s} {'r3 O2':>7s} {'r4 O2':>7s} {'r5 O2':>7s} | {'r1 O3':>7s} {'r2 O3':>7s} {'r3 O3':>7s} {'r4 O3':>7s} {'r5 O3':>7s}")
    for row in results:
        o2_runs = row['o2_times'] + [None] * (RUNS - len(row['o2_times']))
        o3_runs = row['o3_times'] + [None] * (RUNS - len(row['o3_times']))
        o2_cells = " ".join(f"{v:>7.3f}" if v is not None else f"{'-':>7s}" for v in o2_runs)
        o3_cells = " ".join(f"{v:>7.3f}" if v is not None else f"{'-':>7s}" for v in o3_runs)
        print(f"{row['label']:<18s} | {o2_cells} | {o3_cells}")

    print("\nCOMPARISON vs previous (VM_BENCH_20260720.md)")
    print("-" * 100)
    print(f"{'测试':<18s} | {'O2 now':>8s} {'O2 prev':>8s} {'Δ%':>8s} | {'O3 now':>8s} {'O3 prev':>8s} {'Δ%':>8s} | 备注")
    for row in results:
        o2_now = row['o2'] if row['o2'] is not None else 0
        o3_now = row['o3'] if row['o3'] is not None else 0
        o2_prev = row['prev_o2']
        o3_prev = row['prev_o3']
        o2_dpct = (o2_now - o2_prev) / o2_prev * 100 if o2_prev else 0
        o3_dpct = (o3_now - o3_prev) / o3_prev * 100 if o3_prev else 0
        note = ""
        if row['label'] == "DIV 1M/500k" and o2_now > o2_prev * 1.10:
            note = "[REGRESSION >10%]"
        elif abs(o2_dpct) > 20 or abs(o3_dpct) > 20:
            note = "[>20% delta]"
        print(f"{row['label']:<18s} | {o2_now:>8.3f} {o2_prev:>8.3f} {o2_dpct:>+7.1f}% | {o3_now:>8.3f} {o3_prev:>8.3f} {o3_dpct:>+7.1f}% | {note}")

    print("\nTOTAL USER TIME (s) per run, for cross-check")
    print("-" * 100)
    for row in results:
        o2_tot = ", ".join(f"{t:.4f}" for t in row['o2_totals']) if row['o2_totals'] else "-"
        o3_tot = ", ".join(f"{t:.4f}" for t in row['o3_totals']) if row['o3_totals'] else "-"
        print(f"{row['label']:<18s} | O2: [{o2_tot}]")
        print(f"{'':<18s} | O3: [{o3_tot}]")

    client.close()
    print("\n=== DONE ===")


if __name__ == "__main__":
    main()
