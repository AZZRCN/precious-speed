import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

cmd = (
    "echo '=== tools ==='; which g++; g++ --version | head -1; which perf; nproc; "
    "echo '=== hexbench data ==='; ls ~/hexbench/data/div/ 2>/dev/null | head; "
    "echo '=== divbench ==='; ls -d ~/divbench 2>/dev/null; ls ~/divbench 2>/dev/null | head; "
    "echo '=== home ==='; ls ~ | head -40"
)
rc, out, err = run(cmd, timeout=60)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:800])
