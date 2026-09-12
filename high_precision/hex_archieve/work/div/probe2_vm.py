import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

cmd = (
    "echo '=== sizes (bytes) ==='; "
    "wc -c ~/hexbench/data/div/*.in | sort -n | tail -4; "
    "echo '=== hexcheck.py ==='; "
    "ls -la ~/divbench/hexcheck.py; head -1 ~/divbench/hexcheck.py; which python3; "
    "echo '=== full data list ==='; ls ~/hexbench/data/div/ | sed 's/.in//;s/.exp//' | sort -u"
)
rc, out, err = run(cmd, timeout=60)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1200])
