import sys, re
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import put, run

src = open(r"D:\hex_precious_speed\submit_ready\div.cpp", encoding="utf-8").read()
repls = [
    ("static void difFlat(cpx* d, u32 n, u32 bb) {",
     "static void __attribute__((noinline)) difFlat(cpx* d, u32 n, u32 bb) {"),
    ("static void ditFlat(cpx* d, u32 n, u32 bb) {",
     "static void __attribute__((noinline)) ditFlat(cpx* d, u32 n, u32 bb) {"),
    ("static void pointwise(cpx* F, cpx* G, u32 n) {",
     "static void __attribute__((noinline)) pointwise(cpx* F, cpx* G, u32 n) {"),
    ("static void split_b2(const u64* src, double* g, size_t n, int k) {",
     "static void __attribute__((noinline)) split_b2(const u64* src, double* g, size_t n, int k) {"),
]
for a, b in repls:
    assert a in src, "missing: " + a
    src = src.replace(a, b)
open(r"D:\hex_precious_speed\work\div\div_v11_ni.cpp", "w", encoding="utf-8").write(src)
print("patched 4 functions -> div_v11_ni.cpp")

put(r"D:\hex_precious_speed\work\div\div_v11_ni.cpp", "/tmp/div_v11_ni.cpp")

cmd = (
    "cd /tmp; "
    "g++ -O2 -march=native -std=c++23 -g -o div_v11_ni div_v11_ni.cpp 2>&1 | tail -3; echo BUILD_RC=$?; "
    "LARGEST=$(ls -S ~/hexbench/data/div/*.in | head -1); echo LARGEST=$LARGEST; "
    "python3 ~/divbench/hexcheck.py run ./div_v11_ni \"$LARGEST\" \"${LARGEST%.in}.exp\"; echo CK_RC=$?; "
    "perf record -e instructions:u -F 5000 -o /tmp/pni.data bash -c "
    "'for i in $(seq 30); do ./div_v11_ni < /home/azzr/hexbench/data/div/small_00.in > /dev/null; done' "
    "2> /tmp/recni.txt; echo REC_RC=$?; cat /tmp/recni.txt; "
    "echo '=== flat self% per function ==='; "
    "perf report -i /tmp/pni.data --stdio --no-children --percent-limit=0.2 2>/dev/null | head -80"
)
rc, out, err = run(cmd, timeout=150)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:1500])
