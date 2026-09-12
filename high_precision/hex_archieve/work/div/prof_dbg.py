import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

cmd = (
    "cd /tmp; "
    "echo '=== symbols of interest ==='; nm div_v11_ni 2>/dev/null | grep -Ei ' (difFlat|ditFlat|pointwise|split_b2|convolve|solve|fft|main|read|parse|print|write)' | head -40; "
    "echo '=== default report (all symbols, children) ==='; perf report -i /tmp/pni.data --stdio --percent-limit=0.0 2>/dev/null | head -60; "
    "echo '=== call graph (top) ==='; perf report -i /tmp/pni.data --stdio -g --percent-limit=1.0 2>/dev/null | head -50"
)
rc, out, err = run(cmd, timeout=90)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
