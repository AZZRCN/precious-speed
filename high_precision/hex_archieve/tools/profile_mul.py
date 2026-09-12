import vm, os

LOCAL = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "submit_ready", "mul.cpp"))
vm.put(LOCAL, "/tmp/mul_profile.cpp")

cmd = r'''
cd /tmp
echo "=== build (-march=x86-64-v3, no-inline-small-functions) ==="
g++ -O2 -march=x86-64-v3 -fno-inline-small-functions -fno-inline -g -o mul_profile mul_profile.cpp 2>&1 | tail -3
echo "BUILD_RC=$?"
echo "=== locate checker ==="
CK=$(find ~ -name hexcheck.py 2>/dev/null | head -1); echo "CHECKER=$CK"
echo "=== AC verify max_max_00 ==="
python3 "$CK" run ./mul_profile ~/hexbench/data/mul/max_max_00.in ~/hexbench/data/mul/max_max_00.exp 2>&1 | tail -3
echo "=== perf build instrs (single warm) ==="
perf stat -e instructions:u -- ./mul_profile < ~/hexbench/data/mul/max_max_00.in > /dev/null 2>&1 | grep -E "instructions"
echo "=== perf record 30 iters ==="
perf record -e instructions:u -F 2000 -o /tmp/mul_pg.data -- bash -c 'for i in $(seq 30); do ./mul_profile < ~/hexbench/data/mul/max_max_00.in > /dev/null; done' 2>&1 | tail -2
echo "=== perf report (sorted by self instr) ==="
perf report -i /tmp/mul_pg.data --stdio --no-children --percent-limit=0.3 2>/dev/null | head -55
'''
rc, out, err = vm.run(cmd, timeout=240)
print("RC", rc)
print(out)
if err.strip():
    print("ERR", err[:600])
