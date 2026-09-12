#!/usr/bin/env python3
# bench.py - the reducer's measurement + correctness gate, running on the local Intel VM
# (192.168.1.66, azzr/1234). The VM is a PROXY rig: instruction-fetch (I refs) counts
# are cross-arch stable for the same x86-64 binary, so they predict the AMD Zen3 judge.
#
# Subcommands:
#   build NAME SRC          compile SRC -> bin/NAME on VM  (-O3 -march=haswell = AVX2/FMA/BMI2, matches Zen3)
#   oracle NAME REF CASES   byte-compare NAME vs REF outputs on CASES  (equivalence gate)
#   ir    NAME CASES        callgrind I refs of NAME on CASES.big (instruction-count metric)
#   run   NAME SRC REF CASES   build NAME(+REF) + oracle + ir   (one-shot candidate eval)
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) + "/..")
from tools.vm_ssh import run, put

ROOT = "/tmp/hexcmp"
CFLAGS = "-O3 -march=haswell -std=c++17 -fno-stack-protector"

def build(name, src):
    base = os.path.basename(src)
    put(src, f"{ROOT}/src/{base}")
    rc, o, e = run(f"cd {ROOT} && mkdir -p bin src && "
                   f"g++ {CFLAGS} -o bin/{name} src/{base} 2>bin/{name}.err; echo EXIT=$?")
    if "EXIT=0" not in o:
        rc2, o2, e2 = run(f"cat {ROOT}/bin/{name}.err")
        print("BUILD FAIL", name)
        print(o2)
        return False
    print("built", name)
    return True

def oracle(name, ref, cases):
    rc1, o1, e1 = run(f"cd {ROOT} && ./bin/{ref} < {cases} > /tmp/o_ref.txt 2>/dev/null; echo EXIT=$?")
    rc2, o2, e2 = run(f"cd {ROOT} && ./bin/{name} < {cases} > /tmp/o_opt.txt 2>/dev/null; echo EXIT=$?")
    if "EXIT=0" not in o1:
        print("REF FAIL"); print(e1); return None
    if "EXIT=0" not in o2:
        rc3,o3,e3 = run(f"cat /tmp/o_opt.txt; echo '---ERR---'; {ROOT}/bin/{name}.err 2>/dev/null; cd {ROOT} && ./bin/{name} < {cases} 2>&1 | head -20")
        print("OPT FAIL"); print(o3); return None
    rc, o, e = run("cmp -s /tmp/o_ref.txt /tmp/o_opt.txt && echo SAME || echo DIFF")
    if "SAME" in o:
        print(f"oracle: SAME ({name} == {ref}) on {cases}")
        return 0
    # count differing lines
    rc, od, ed = run("diff -y --suppress-common-lines /tmp/o_ref.txt /tmp/o_opt.txt | wc -l")
    print(f"oracle: DIFF ({name} != {ref}) on {cases}; differing lines={od.strip()}")
    return int(od.strip() or 1)

def oracledir(name, ref, casedir):
    # remote batch: run ref and opt on every *.in, byte-compare, count diffs
    script = f"""#!/bin/bash
fails=0; total=0
for f in {casedir}/*.in; do
  total=$((total+1))
  ./{name} < "$f" > /tmp/o_opt.txt 2>/dev/null
  rc_opt=$?
  ./{ref} < "$f" > /tmp/o_ref.txt 2>/dev/null
  rc_ref=$?
  if [ $rc_opt -ne 0 ] || [ $rc_ref -ne 0 ]; then fails=$((fails+1)); echo "CRASH $f opt=$rc_opt ref=$rc_ref"; continue; fi
  if ! cmp -s /tmp/o_ref.txt /tmp/o_opt.txt; then fails=$((fails+1)); echo "DIFF $f"; fi
done
echo "ORACLE_DONE total=$total fails=$fails"
"""
    local = os.path.join(os.path.dirname(os.path.abspath(__file__)), "_oracle.sh")
    with open(local, "w", newline="\n") as f:
        f.write(script)
    put(local, f"{ROOT}/bin/_oracle.sh")
    rc, o, e = run(f"cd {ROOT}/bin && bash _oracle.sh")
    print(o)
    return o

def ir(name, cases):
    big = cases + ".big"
    rc, o, e = run(f"cd {ROOT} && rm -f cg_{name}.out && "
                   f"valgrind --tool=callgrind --cache-sim=yes --callgrind-out-file=cg_{name}.out "
                   f"./bin/{name} < {big} >/dev/null 2>cg_{name}.log; echo EXIT=$?")
    # parse "I   refs:" from valgrind stderr
    iref = ""
    for line in e.splitlines():
        if "I   refs:" in line or "I refs:" in line:
            iref = line.strip()
            break
    if not iref:
        rc2,o2,e2 = run(f"grep -E 'I   refs|I refs' {ROOT}/cg_{name}.log | head")
        iref = o2.strip() or "I refs: (not found)"
    print(f"ir[{name}] on {big}: {iref}")
    return iref

def main():
    op = sys.argv[1]
    if op == "build":
        build(sys.argv[2], sys.argv[3])
    elif op == "oracle":
        oracle(sys.argv[2], sys.argv[3], sys.argv[4])
    elif op == "ir":
        ir(sys.argv[2], sys.argv[3])
    elif op == "oracledir":
        oracledir(sys.argv[2], sys.argv[3], sys.argv[4])
    elif op == "run":
        name, src, ref, cases = sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]
        if not build(name, src): return
        # build ref only if its binary missing or source newer not tracked; just (re)build
        if not build(ref, sys.argv[6] if len(sys.argv) > 6 else ref):
            # ref src provided separately? we just need ref binary to exist
            pass
        f = oracle(name, ref, cases)
        iref = ir(name, cases)
        print("RESULT", name, "failures=", f, iref)
    else:
        print("unknown op")

if __name__ == "__main__":
    main()
