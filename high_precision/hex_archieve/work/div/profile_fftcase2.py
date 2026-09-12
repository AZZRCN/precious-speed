import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

cmd = (
    "cd /tmp; "
    "perf record -e instructions:u -F 2000 -o /tmp/pg2.data bash -c "
    "'for i in $(seq 30); do ./div_v11_g2 < /home/azzr/hexbench/data/div/a_max_b_random_00.in > /dev/null; done' "
    "2> /tmp/rec.txt; echo REC_RC=$?; cat /tmp/rec.txt; "
    "echo '=== flat self% per function (FFT-heavy case) ==='; "
    "perf report -i /tmp/pg2.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -55"
)
rc, out, err = run(cmd, timeout=120)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:2000])
