import vm, os

LOCAL = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "submit_ready", "mul.cpp"))
src = open(LOCAL).read()

# patch pick_k: bump k by 2 for large inputs (cap 16) -> shorter FFT + fewer merge iters
old = "    int i = 0;\n    while (u > gk[i]) ++i;\n    return 19 - i;\n"
new = ("    int i = 0;\n    while (u > gk[i]) ++i;\n"
       "    int k = 19 - i;\n"
       "    if (u >= (size_t)14 << 14) k += 2;   // 大输入抬 k: 缩 FFT + 减 merge 迭代\n"
       "    if (k > 16) k = 16;\n"
       "    return k;\n")
assert old in src, "pick_k pattern not found"
patched = src.replace(old, new)

open("/tmp/mul_baseline.cpp", "w").write(src)
open("/tmp/mul_kbias.cpp", "w").write(patched)
vm.put("/tmp/mul_baseline.cpp", "/tmp/mul_baseline.cpp")
vm.put("/tmp/mul_kbias.cpp", "/tmp/mul_kbias.cpp")

cmd = r'''
cd /tmp
CK=~/hexbench/oracle/hexcheck.py
echo "=== build baseline ==="
g++ -O2 -march=x86-64-v3 -fno-inline-small-functions -fno-inline -g -o mul_baseline mul_baseline.cpp 2>&1 | tail -1
echo "=== build kbias ==="
g++ -O2 -march=x86-64-v3 -fno-inline-small-functions -fno-inline -g -o mul_kbias mul_kbias.cpp 2>&1 | tail -1
echo "=== 22/22 verify baseline ==="
ok=0; bad=0
for f in ~/hexbench/data/mul/*.in; do e="${f%.in}.exp"; r=$(python3 "$CK" run ./mul_baseline "$f" "$e" 2>&1 | tail -1); if echo "$r" | grep -q OK; then ok=$((ok+1)); else bad=$((bad+1)); echo "BASE FAIL $(basename $f): $r"; fi; done
echo "BASELINE_OK=$ok BASELINE_BAD=$bad"
echo "=== 22/22 verify kbias ==="
ok=0; bad=0
for f in ~/hexbench/data/mul/*.in; do e="${f%.in}.exp"; r=$(python3 "$CK" run ./mul_kbias "$f" "$e" 2>&1 | tail -1); if echo "$r" | grep -q OK; then ok=$((ok+1)); else bad=$((bad+1)); echo "KBIAS FAIL $(basename $f): $r"; fi; done
echo "KBIAS_OK=$ok KBIAS_BAD=$bad"
echo "=== perf stat single max_max_00 baseline ==="
perf stat -e instructions:u -- ./mul_baseline < ~/hexbench/data/mul/max_max_00.in > /dev/null 2>sb.txt; grep instructions sb.txt
echo "=== perf stat single max_max_00 kbias ==="
perf stat -e instructions:u -- ./mul_kbias < ~/hexbench/data/mul/max_max_00.in > /dev/null 2>sk.txt; grep instructions sk.txt
echo "=== perf report kbias (hotspot) ==="
perf record -e instructions:u -F 2000 -o /tmp/mul_kb.data -- bash -c 'for i in $(seq 20); do ./mul_kbias < ~/hexbench/data/mul/max_max_00.in > /dev/null; done' 2>&1 | tail -1
perf report -i /tmp/mul_kb.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -25
'''
rc, out, err = vm.run(cmd, timeout=300)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:800])
