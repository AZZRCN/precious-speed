import sys
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

src = open(r"D:\hex_precious_speed\submit_ready\div.cpp", encoding="utf-8").read()
# remove file-local 'static ' so every function becomes a global symbol perf can resolve
# (do NOT touch 'static_cast'); then build fully-noinline for clean per-function attribution
n = src.count("static ")
src2 = src.replace("static ", "")
assert n > 0 and len(src2) == len(src) - 7 * n, (n, len(src), len(src2))
open(r"D:\hex_precious_speed\work\div\div_v11_g.cpp", "w", encoding="utf-8").write(src2)
print(f"stripped {n} 'static ' keywords -> div_v11_g.cpp")

put(r"D:\hex_precious_speed\work\div\div_v11_g.cpp", "/tmp/div_v11_g.cpp")

cmd = (
    "cd /tmp; "
    "g++ -O2 -march=native -std=c++23 -g -fno-inline -fno-inline-functions -o div_v11_g div_v11_g.cpp 2>&1 | tail -5; echo BUILD_RC=$?; "
    "LARGEST=$(ls -S ~/hexbench/data/div/*.in | head -1); echo LARGEST=$LARGEST; "
    "python3 ~/divbench/hexcheck.py run ./div_v11_g \"$LARGEST\" \"${LARGEST%.in}.exp\"; echo CK_RC=$?; "
    "perf record -e instructions:u -F 5000 -o /tmp/pg.data bash -c "
    "'for i in $(seq 20); do ./div_v11_g < /home/azzr/hexbench/data/div/small_00.in > /dev/null; done' "
    "2> /tmp/recg.txt; echo REC_RC=$?; cat /tmp/recg.txt; "
    "echo '=== flat self% per function (top 25) ==='; "
    "perf report -i /tmp/pg.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -45"
)
rc, out, err = run(cmd, timeout=150)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:2000])
