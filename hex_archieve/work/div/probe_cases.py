import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

cases = ["small_00", "max_00", "a_max_b_random_00", "large_00", "medium_00", "length_ratioteger_00"]
parts = []
parts.append("cd /tmp; ls -la div_v11 2>/dev/null | awk '{print $5, $9}'; echo '--- per-case T and instructions:u ---';")
for c in cases:
    parts.append(
        f"echo '=== {c} ==='; "
        f"head -c 24 ~/hexbench/data/div/{c}.in | tr -d '\\n'; echo; "
        f"perf stat -e instructions:u -r 2 ./div_v11 < ~/hexbench/data/div/{c}.in > /dev/null 2>/tmp/s_{c}.txt; "
        f"grep -E 'instructions' /tmp/s_{c}.txt | head -1"
    )
cmd = "\n".join(parts)
rc, out, err = run(cmd, timeout=120)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
