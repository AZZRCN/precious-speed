#!/usr/bin/env python3
"""
Re-measure max RSS for all (binary, infile) pairs.
Fix: /usr/bin/time -v writes to stderr; need `2>&1 > /dev/null` order
     (stderr -> pipe first, then stdout -> /dev/null).
"""
import re
import paramiko

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"
REMOTE_DIR = "/tmp/bench"

# (label, binary, infile)
PAIRS = [
    ("ADD 1M+1M (O2)",     "moptm_add",    "add_1M.in"),
    ("ADD 1M+1M (O3)",     "moptm_add_o3", "add_1M.in"),
    ("MUL 500k*500k (O2)", "moptm_mul",    "mul_500k.in"),
    ("MUL 500k*500k (O3)", "moptm_mul_o3", "mul_500k.in"),
    ("DIV 1M/500k (O2)",   "moptm_div",    "div_1M_500k.in"),
    ("DIV 1M/500k (O3)",   "moptm_div_o3", "div_1M_500k.in"),
    ("DIV 200k/100k (O2)", "moptm_div",    "div_200k_100k.in"),
    ("DIV 200k/100k (O3)", "moptm_div_o3", "div_200k_100k.in"),
]


def get_client():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PWD, timeout=15)
    return c


def run(client, cmd, timeout=300):
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err


def measure_max_rss(client, binary, infile):
    """Correct redirection: `2>&1 > /dev/null` so stderr (time -v output) goes to the pipe."""
    cmd = (f"cd {REMOTE_DIR} && /usr/bin/time -v ./{binary} < {infile} 2>&1 > /dev/null "
           f"| grep -E 'Maximum resident|Exit status'")
    rc, out, _ = run(client, cmd, timeout=300)
    m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", out)
    es = re.search(r"Exit status:\s*(-?\d+)", out)
    rss = int(m.group(1)) if m else None
    exit_code = int(es.group(1)) if es else None
    return rss, exit_code, out


def main():
    print(f"Connecting to {USER}@{HOST} ...")
    client = get_client()
    print("[OK] connected\n")

    print(f"{'Label':<22s} | {'Binary':<14s} | {'RSS (kB)':>10s} | {'RSS (MB)':>9s} | exit | raw")
    print("-" * 90)
    results = []
    for label, binary, infile in PAIRS:
        rss, es, raw = measure_max_rss(client, binary, infile)
        rss_mb = f"{rss/1024:.1f}" if rss else "-"
        rss_kb_str = f"{rss}" if rss else "None"
        raw_short = raw.strip().replace("\n", " | ")[:80]
        print(f"{label:<22s} | {binary:<14s} | {rss_kb_str:>10s} | {rss_mb:>9s} | {es} | {raw_short}")
        results.append((label, binary, rss, es))

    # Summary table aligned with bench cases
    print("\n=== RSS summary aligned to bench cases ===")
    print(f"{'测试':<18s} | {'O2 RSS (MB)':>12s} | {'O3 RSS (MB)':>12s}")
    print("-" * 50)
    cases = [
        ("ADD 1M+1M",       "moptm_add",    "moptm_add_o3"),
        ("MUL 500k*500k",   "moptm_mul",    "moptm_mul_o3"),
        ("DIV 1M/500k",     "moptm_div",    "moptm_div_o3"),
        ("DIV 200k/100k",   "moptm_div",    "moptm_div_o3"),
    ]
    for case_label, o2_bin, o3_bin in cases:
        o2_rss = None
        o3_rss = None
        for label, binary, rss, es in results:
            if label.startswith(case_label) and binary == o2_bin:
                o2_rss = rss
            if label.startswith(case_label) and binary == o3_bin:
                o3_rss = rss
        o2_mb = f"{o2_rss/1024:.1f}" if o2_rss else "-"
        o3_mb = f"{o3_rss/1024:.1f}" if o3_rss else "-"
        print(f"{case_label:<18s} | {o2_mb:>12s} | {o3_mb:>12s}")

    client.close()
    print("\n=== DONE ===")


if __name__ == "__main__":
    main()
