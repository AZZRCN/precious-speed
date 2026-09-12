"""Verify the new VM can still do the essentials even without vPMC:
  - g++ -std=c++23 -march=x86-64-v3 compile of the real D47 source
  - wall-clock timing fallback
  - disk headroom warning
Read-mostly: writes only into /tmp/envchk.
"""
import sys
import os
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("VM_HOST", "192.168.1.64")

from vmctl import run, put, VM  # noqa: E402

LOCAL_SRC = r"D:\precious_speed\best\div_D47.cpp"
REMOTE_DIR = "/tmp/envchk"
REMOTE_SRC = REMOTE_DIR + "/div_D47.cpp"

run("mkdir -p " + REMOTE_DIR)
print("[1/3] uploading div_D47.cpp ...")
t0 = time.time()
put(LOCAL_SRC, REMOTE_SRC)
print("      done in %.1fs" % (time.time() - t0))

print("[2/3] compiling with -std=c++23 -march=x86-64-v3 ...")
t0 = time.time()
rc, out, err = run(
    "cd %s && time g++ -O2 -std=c++23 -march=x86-64-v3 "
    "-o d47 div_D47.cpp 2>&1 | tail -20" % REMOTE_DIR
)
print(out[-1500:] if out else "(no output)")
print("      wall %.1fs" % (time.time() - t0))

print("[3/3] disk + core sanity ...")
rc, out, err = run("\n".join([
    "ls -la %s/d47 2>/dev/null | awk '{print $5, $9}'" % REMOTE_DIR,
    "echo ---DISK---",
    "df -h / /tmp | tail -3",
    "echo ---GCCWORK---",
    "du -sh /home/azzr/gcc-work 2>/dev/null",
    "echo ---NPROC---",
    "nproc",
]))
print(out)
print("HOST =", VM["hostname"])
