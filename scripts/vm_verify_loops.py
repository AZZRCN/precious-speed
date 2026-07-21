#!/usr/bin/env python3
"""Verify benchmark numbers with different LOOPS values."""
import paramiko
import re

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect("192.168.1.55", username="azzr", password="1234", timeout=15)


def run(cmd, timeout=300):
    _, o, e = c.exec_command(cmd, timeout=timeout)
    return o.read().decode(errors="replace"), e.read().decode(errors="replace")


def extract_user_ms(s):
    m = re.search(r"User time \(seconds\):\s*([\d.]+)", s)
    return float(m.group(1)) * 1000.0 if m else None


def extract_elapsed_ms(s):
    # Format: "Elapsed (wall clock) time (h:mm:ss or m:ss): 0:00.15"
    m = re.search(r"Elapsed \(wall clock\) time \([^)]*\):\s*(\d+):([\d.]+)", s)
    if m:
        # m:ss.cc or h:mm:ss.cc - we only capture two groups, treat as m:ss
        mn, sec = int(m.group(1)), float(m.group(2))
        return (mn * 60 + sec) * 1000.0
    return None


cases = [
    ("ADD 1M", "moptm_add", "add_1M.in"),
    ("ADD 1M O3", "moptm_add_o3", "add_1M.in"),
    ("MUL 500k", "moptm_mul", "mul_500k.in"),
    ("MUL 500k O3", "moptm_mul_o3", "mul_500k.in"),
    ("DIV 1M/500k", "moptm_div", "div_1M_500k.in"),
    ("DIV 1M/500k O3", "moptm_div_o3", "div_1M_500k.in"),
    ("DIV 200k/100k", "moptm_div", "div_200k_100k.in"),
]

for loops in [50, 100]:
    print(f"\n========== LOOPS={loops} ==========")
    for label, binary, infile in cases:
        inner = f"for i in $(seq 1 {loops}); do ./{binary} < {infile} > /dev/null; done"
        cmd = f"cd /tmp/bench && /usr/bin/time -v bash -c '{inner}' 2>&1 | grep -E 'User time|Elapsed|Maximum'"
        out, _ = run(cmd, timeout=600)
        u = extract_user_ms(out)
        w = extract_elapsed_ms(out)
        if u is not None:
            print(f"  {label:20s}  total_user={u:7.1f}ms  per_run_user={u/loops:6.3f}ms  wall={w:7.1f}ms  per_run_wall={w/loops if w else 0:6.3f}ms")
        else:
            print(f"  {label:20s}  FAIL: {out!r}")

c.close()
print("DONE")
