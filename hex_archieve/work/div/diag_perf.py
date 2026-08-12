import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

cmd = (
    "cd /tmp; "
    "LARGEST=$(ls -S ~/hexbench/data/div/*.in | head -1); echo LARGEST=$LARGEST; "
    "echo '=== plain run ==='; ./div_v11 < \"$LARGEST\" > /dev/null; echo PLAIN_RC=$?; "
    "echo '=== perf stat once ==='; perf stat -e instructions:u ./div_v11 < \"$LARGEST\" > /dev/null 2> /tmp/s2.txt; echo PS_RC=$?; cat /tmp/s2.txt; "
    "echo '=== perf record freq ==='; perf record -e instructions:u -F 9900 -o /tmp/p.data ./div_v11 < \"$LARGEST\" > /dev/null 2> /tmp/r2.txt; echo PR_RC=$?; cat /tmp/r2.txt; "
    "perf report -i /tmp/p.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -60"
)
rc, out, err = run(cmd, timeout=120)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
