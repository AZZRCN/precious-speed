import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

# original (inlined) source already at /tmp/div_v11.cpp; build fully-noinline for symbolization
cmd = (
    "cd /tmp; "
    "g++ -O2 -march=native -std=c++23 -g -fno-inline -fno-inline-functions -o div_v11_fn div_v11.cpp 2>&1 | tail -3; echo BUILD_RC=$?; "
    "LARGEST=$(ls -S ~/hexbench/data/div/*.in | head -1); echo LARGEST=$LARGEST; "
    "python3 ~/divbench/hexcheck.py run ./div_v11_fn \"$LARGEST\" \"${LARGEST%.in}.exp\"; echo CK_RC=$?; "
    "perf record -e instructions:u -F 5000 -o /tmp/pfn.data bash -c "
    "'for i in $(seq 25); do ./div_v11_fn < /home/azzr/hexbench/data/div/small_00.in > /dev/null; done' "
    "2> /tmp/recfn.txt; echo REC_RC=$?; cat /tmp/recfn.txt; "
    "echo '=== flat self% per function ==='; "
    "perf report -i /tmp/pfn.data --stdio --no-children --percent-limit=0.2 2>/dev/null | head -80"
)
rc, out, err = run(cmd, timeout=150)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
