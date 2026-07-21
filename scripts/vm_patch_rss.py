#!/usr/bin/env python3
"""Patch max RSS values into the existing report - re-measure with LC_ALL=C."""
import paramiko
import re

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"
REMOTE_DIR = "/tmp/bench"
LOCAL_REPORT = r"d:\precious_speed\VM_BENCH_20260720.md"

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password="1234", timeout=15)


def run(cmd, timeout=300):
    _, o, e = c.exec_command(cmd, timeout=timeout)
    return o.read().decode(errors="replace"), e.read().decode(errors="replace")


# LC_ALL=C ensures /usr/bin/time -v output is in English (default locale)
cases = [
    ("ADD 1M+1M",       "moptm_add",    "moptm_add_o3",   "add_1M.in"),
    ("MUL 500k*500k",   "moptm_mul",    "moptm_mul_o3",   "mul_500k.in"),
    ("DIV 1M/500k",     "moptm_div",    "moptm_div_o3",   "div_1M_500k.in"),
    ("DIV 200k/100k",   "moptm_div",    "moptm_div_o3",   "div_200k_100k.in"),
]

print("Measuring max RSS with LC_ALL=C ...")
rss_map = {}  # label -> (o2_rss, o3_rss)
for label, bin_o2, bin_o3, infile in cases:
    rss_vals = {}
    for opt_key, binname in [("o2", bin_o2), ("o3", bin_o3)]:
        # Use temp file to capture /usr/bin/time -v output (avoids pipe+redirect ordering issues)
        # Note: `cmd > /dev/null 2>&1` sends both stdout AND stderr to /dev/null - WRONG
        # Correct: `cmd > /dev/null 2> /tmp/t.txt` - stdout to /dev/null, stderr to temp file
        cmd = (f"cd {REMOTE_DIR} && LC_ALL=C /usr/bin/time -v ./{binname} < {infile} "
               f"> /dev/null 2> /tmp/rss_tmp.txt; "
               f"LC_ALL=C grep -E 'Maximum resident|Exit status' /tmp/rss_tmp.txt")
        out, _ = run(cmd, timeout=300)
        m = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", out)
        rss = int(m.group(1)) if m else None
        rss_vals[opt_key] = rss
        print(f"  {label:18s} [{opt_key}] {binname}: max_rss={rss} kB  (raw: {out.strip()!r})")
    rss_map[label] = (rss_vals["o2"], rss_vals["o3"])

c.close()

# Patch the report
with open(LOCAL_REPORT, "r", encoding="utf-8") as f:
    report = f.read()

# Replace each row's "- | -" with actual values for max RSS columns
for label, (o2_rss, o3_rss) in rss_map.items():
    o2_mb = f"{o2_rss/1024:.1f}" if o2_rss else "-"
    o3_mb = f"{o3_rss/1024:.1f}" if o3_rss else "-"
    # Match pattern: "| ADD 1M+1M | X.XXX | Y.YYY | Z.ZZZ | - | - |"
    # Replace the last "- | -" with the actual values
    pattern = re.compile(rf"(\| {re.escape(label)} \| [^|]+ \| [^|]+ \| [^|]+ \|) - \| - \|")
    new_str = rf"\1 {o2_mb} | {o3_mb} |"
    new_report = pattern.sub(new_str, report)
    if new_report == report:
        print(f"  [WARN] pattern not found for label={label!r}")
    report = new_report

with open(LOCAL_REPORT, "w", encoding="utf-8") as f:
    f.write(report)

print("\n--- Patched report ---")
print(report)
print("\nDONE")
