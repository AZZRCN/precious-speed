import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

put(r"D:\hex_precious_speed\submit_ready\div.cpp", "/tmp/div_v11.cpp")

cmd = (
    "cd /tmp && "
    "g++ -O2 -march=native -std=c++23 -g -o div_v11 div_v11.cpp 2>&1 | tail -3; "
    "echo BUILD_DONE; "
    "LARGEST=$(ls -S ~/hexbench/data/div/*.in | head -1); echo LARGEST=$LARGEST; "
    "echo '=== correctness ==='; python3 ~/divbench/hexcheck.py run ./div_v11 \"$LARGEST\" \"${LARGEST%.in}.exp\"; echo CK_RC=$?; "
    "echo '=== perf stat (instructions:u, -r 3) ==='; "
    "perf stat -e instructions:u -r 3 ./div_v11 < \"$LARGEST\" > /dev/null 2> /tmp/stat.txt; cat /tmp/stat.txt; "
    "echo '=== perf record (self %) ==='; "
    "perf record -e instructions:u -c 1000 -o /tmp/p.data ./div_v11 < \"$LARGEST\" > /dev/null 2>&1; "
    "perf report -i /tmp/p.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -70"
)
rc, out, err = run(cmd, timeout=150)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
