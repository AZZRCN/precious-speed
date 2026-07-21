#!/usr/bin/env python3
"""Use bash builtin `time` for millisecond precision on single direct runs."""
import paramiko
import re

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect("192.168.1.55", username="azzr", password="1234", timeout=15)


def run(cmd, timeout=300):
    _, o, e = c.exec_command(cmd, timeout=timeout)
    return o.read().decode(errors="replace"), e.read().decode(errors="replace")


# bash builtin time outputs to stderr
# TIMEFORMAT="%R %U %S" => real user sys (each in seconds with 3 decimal places)
cases = [
    ("ADD 1M+1M O2",        "moptm_add",     "add_1M.in"),
    ("ADD 1M+1M O3",        "moptm_add_o3",   "add_1M.in"),
    ("MUL 500k*500k O2",    "moptm_mul",      "mul_500k.in"),
    ("MUL 500k*500k O3",    "moptm_mul_o3",   "mul_500k.in"),
    ("DIV 1M/500k O2",      "moptm_div",      "div_1M_500k.in"),
    ("DIV 1M/500k O3",      "moptm_div_o3",   "div_1M_500k.in"),
    ("DIV 200k/100k O2",    "moptm_div",      "div_200k_100k.in"),
    ("DIV 200k/100k O3",    "moptm_div_o3",   "div_200k_100k.in"),
]

print("=== bash builtin time (single direct runs, ms) ===")
print(f"{'case':22s}  {'run1':>10}  {'run2':>10}  {'run3':>10}  {'run4':>10}  {'run5':>10}  {'median_user':>12}")
for label, binary, infile in cases:
    user_times = []
    for i in range(5):
        # bash -c 'TIMEFORMAT=...; time cmd 2>&1' -- the inner time output goes to stderr
        # We want to capture the time output, but redirect the binary's actual output to /dev/null
        cmd = (f"cd /tmp/bench && bash -c '"
               f"TIMEFORMAT=\"%U\"; "
               f"time ./{binary} < {infile} > /dev/null"
               f"' 2>&1")
        out, err = run(cmd, timeout=300)
        # bash builtin time writes its output to stderr (which is captured as out here due to 2>&1)
        # The output is just the user time in seconds, e.g. "0.014"
        lines = [l.strip() for l in out.splitlines() if l.strip()]
        # the last non-empty line should be the timing
        if lines:
            try:
                t_sec = float(lines[-1])
                user_times.append(t_sec * 1000.0)
            except ValueError:
                print(f"  {label} run{i+1}: FAIL parse last line={lines[-1]!r}, full out={out!r}")
    if user_times:
        sorted_t = sorted(user_times)
        median = sorted_t[len(sorted_t) // 2]
        runs_str = "  ".join(f"{t:8.3f}" for t in user_times)
        print(f"{label:22s}  {runs_str}  {median:10.3f}")
    else:
        print(f"{label:22s}  no successful runs")

c.close()
print("DONE")
