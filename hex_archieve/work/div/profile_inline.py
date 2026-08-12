import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

# optimized -g binary already at /tmp/div_v11 (built earlier). Loop 30x for samples.
cmd = (
    "cd /tmp; "
    "perf record -e instructions:u -F 5000 -o /tmp/p.data bash -c "
    "'for i in $(seq 30); do ./div_v11 < /home/azzr/hexbench/data/div/small_00.in > /dev/null; done' "
    "2> /tmp/rec.txt; echo REC_RC=$?; cat /tmp/rec.txt; "
    "echo '=== flat report with inline resolution ==='; "
    "perf report -i /tmp/p.data --stdio --inline --no-children --percent-limit=0.3 2>/dev/null | head -80"
)
rc, out, err = run(cmd, timeout=120)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
