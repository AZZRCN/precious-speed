#!/usr/bin/env python3
"""Profile div.cpp on burnikel_01 to find performance bottleneck."""
import paramiko, base64

HOST = "192.168.1.55"
USER = "azzr"
PWD = "1234"

SCRIPT = r'''
import subprocess, sys, time
sys.set_int_max_str_digits(0)

GXX11 = "/usr/bin/g++-11"

# Generate burnikel_01 input
print("=== Generating burnikel_01 ===")
SUM_OF_CHARACTER_LENGTH = 4000002
base = 1000000000  # seed=1
bb = base
A, B = [], []
lsum = 0
while True:
    quotient = 2 * bb * bb - 1
    remainder = (bb - 1) * bb
    y = remainder + 1
    x = quotient * y + remainder
    sa, sb = str(x), str(y)
    lsum += len(sa) + len(sb)
    if lsum > SUM_OF_CHARACTER_LENGTH:
        break
    A.append(sa); B.append(sb)
    bb *= base
inp = str(len(A)) + "\n" + "\n".join(f"{a} {b}" for a, b in zip(A, B)) + "\n"
with open("/tmp/burnikel_01.in", "w") as f:
    f.write(inp)
print(f"  {len(A)} cases, {len(inp)} bytes")

# Show case size distribution
sizes = [(len(a), len(b)) for a, b in zip(A, B)]
print(f"  Case sizes: min={min(s[0] for s in sizes)}..{max(s[0] for s in sizes)} digits A, min={min(s[1] for s in sizes)}..{max(s[1] for s in sizes)} digits B")
print(f"  First 5: {sizes[:5]}")
print(f"  Last 5: {sizes[-5:]}")

# Compile with PROFILE_DIV
print("\n=== Compiling with PROFILE_DIV ===")
r = subprocess.run(
    f"{GXX11} -O2 -std=gnu++20 -static -DONLINE_JUDGE -DPROFILE_DIV /home/azzr/div.cpp -o /home/azzr/div_prof -pthread 2>&1",
    shell=True, capture_output=True, text=True, timeout=120)
if r.returncode != 0:
    print(f"COMPILE FAIL:\n{r.stdout[:3000]}")
    sys.exit(1)
print("[OK] div_prof compiled")

# Run with profile
print("\n=== Running burnikel_01 with profile ===")
t0 = time.time()
r = subprocess.run('/home/azzr/div_prof < /tmp/burnikel_01.in > /dev/null',
                  shell=True, capture_output=True, text=True, timeout=120)
dt = (time.time() - t0) * 1000
print(f"  Wall time: {dt:.1f}ms")
print(f"  Exit code: {r.returncode}")

# Profile output is in prof_detail.log
import re
print("\n=== Profile analysis (prof_detail.log) ===")
try:
    with open("/home/azzr/prof_detail.log") as f:
        prof = f.read()
    print(f"  Log size: {len(prof)} bytes")

    # Count basicMul FALLBACK triggers
    fallbacks_top = re.findall(r"basicMul FALLBACK top_limb=(\d+) \(divid_high=(\d+), inv=(\d+)\)", prof)
    fallbacks_time = re.findall(r"basicMul FALLBACK triggered \(divid_high=(\d+), inv=(\d+)\): ([\d.]+) ms", prof)
    if fallbacks_time:
        print(f"\n=== basicMul FALLBACK stats ===")
        print(f"  Total triggers: {len(fallbacks_time)}")
        fb_times = [float(t) for _, _, t in fallbacks_time]
        print(f"  Total time: {sum(fb_times):.3f} ms")
        print(f"  Max time: {max(fb_times):.3f} ms")
        print(f"  Avg time: {sum(fb_times)/len(fb_times):.3f} ms")
        if fallbacks_top:
            from collections import Counter
            top_dist = Counter(int(t) for t, _, _ in fallbacks_top)
            print(f"  top_limb distribution: {dict(top_dist)}")
        fb_sorted = sorted(fallbacks_time, key=lambda x: -float(x[2]))
        print(f"  Top 5 slowest:")
        for dh, inv, t in fb_sorted[:5]:
            print(f"    divid_high={dh}, inv={inv}, time={t} ms")
    else:
        print("\n  [No basicMul FALLBACK triggered - inv precision optimization working!]")

    # absDivMu totals
    totals = re.findall(r"absDivMu: total: ([\d.]+) ms \(len1=(\d+), len2=(\d+)\)", prof)
    if totals:
        totals_f = [(float(t), int(l1), int(l2)) for t, l1, l2 in totals]
        totals_f.sort(key=lambda x: -x[0])
        print(f"\n=== Top 10 slowest absDivMu cases ===")
        print(f"  {'Time(ms)':>10} {'len1':>8} {'len2':>8}")
        for t, l1, l2 in totals_f[:10]:
            print(f"  {t:>10.3f} {l1:>8} {l2:>8}")
        print(f"\n  Total cases: {len(totals_f)}")
        print(f"  Total time: {sum(t for t,_,_ in totals_f):.1f}ms")
        print(f"  Top 10 time: {sum(t for t,_,_ in totals_f[:10]):.1f}ms ({sum(t for t,_,_ in totals_f[:10])/sum(t for t,_,_ in totals_f)*100:.1f}%)")

    # Show first 2000 chars of raw log
    print(f"\n=== Raw log (first 2000 chars) ===")
    print(prof[:2000])
except Exception as e:
    print(f"[Error reading prof_detail.log: {e}]")

# Also check stderr for PROF_PRINT
if r.stderr:
    print("\n=== Stderr (first 3000 chars) ===")
    print(r.stderr[:3000])

print("[DONE]")
'''

B64 = base64.b64encode(SCRIPT.encode()).decode()
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username=USER, password=PWD, timeout=10)

sftp = c.open_sftp()
sftp.put("d:/precious_speed/div.cpp", "/home/azzr/div.cpp")
sftp.close()
print("[OK] Uploaded div.cpp\n")

stdin, stdout, stderr = c.exec_command(f"cd /home/azzr && echo {B64} | base64 -d | python3 -u", timeout=300)
print(stdout.read().decode())
err = stderr.read().decode()
if err:
    print(f"[STDERR] {err[:500]}")
c.close()
