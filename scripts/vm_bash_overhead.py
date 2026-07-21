#!/usr/bin/env python3
"""Measure pure bash loop overhead (no binary exec)."""
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


# Empty loop overhead
for loops in [50, 100, 500]:
    inner = f"for i in $(seq 1 {loops}); do true; done"
    cmd = f"cd /tmp/bench && /usr/bin/time -v bash -c '{inner}' 2>&1 | grep 'User time'"
    out, _ = run(cmd, timeout=60)
    u = extract_user_ms(out)
    if u is not None:
        print(f"empty loop LOOPS={loops}: user={u}ms  per_iter={u/loops:.4f}ms")
    else:
        print(f"empty loop LOOPS={loops}: user=N/A  out={out!r}")

# Loop that execs a no-op binary (use true as a stub - but true is a builtin in bash)
# Actually let's use /bin/true which is a real binary
print()
for loops in [50, 100]:
    inner = f"for i in $(seq 1 {loops}); do /bin/true < /dev/null > /dev/null; done"
    cmd = f"cd /tmp/bench && /usr/bin/time -v bash -c '{inner}' 2>&1 | grep 'User time'"
    out, _ = run(cmd, timeout=60)
    u = extract_user_ms(out)
    if u is not None:
        print(f"/bin/true exec LOOPS={loops}: user={u}ms  per_iter={u/loops:.4f}ms")
    else:
        print(f"/bin/true exec LOOPS={loops}: user=N/A  out={out!r}")

# Also test with the actual binary but reading from /dev/null (so program exits immediately on empty input)
print()
for loops in [50, 100]:
    inner = f"for i in $(seq 1 {loops}); do ./moptm_add < /dev/null > /dev/null; done"
    cmd = f"cd /tmp/bench && /usr/bin/time -v bash -c '{inner}' 2>&1 | grep 'User time'"
    out, _ = run(cmd, timeout=60)
    u = extract_user_ms(out)
    if u is not None:
        print(f"moptm_add < /dev/null LOOPS={loops}: user={u}ms  per_iter={u/loops:.4f}ms")
    else:
        print(f"moptm_add < /dev/null LOOPS={loops}: user=N/A  out={out!r}")

# Also try with `command time` (bash builtin) for finer precision on direct single runs
print("\n=== bash builtin time, single direct run ===")
for binary, infile in [
    ("moptm_add", "add_1M.in"),
    ("moptm_mul", "mul_500k.in"),
    ("moptm_div", "div_1M_500k.in"),
]:
    cmd = f"cd /tmp/bench && bash -c 'TIMEFORMAT=\"%R %U %S\"; time ./{binary} < {infile} > /dev/null'"
    out, _ = run(cmd, timeout=120)
    print(f"  {binary:18s} < {infile}: {out.strip()}")

c.close()
print("DONE")
