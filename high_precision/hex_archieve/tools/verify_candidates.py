import vm, sys, os

# HEX 道：对现役三候选做 VM 官方 .in/.exp 闸门验证（权威正确性收口）
# mul -> 22/22, div -> 27/27, add -> 22/22
LOCAL = {
    "mul": r"D:\hex_precious_speed\submit_ready\mul.cpp",
    "div": r"D:\hex_precious_speed\submit_ready\div.cpp",
    "add": r"D:\hex_precious_speed\submit_ready\add.cpp",
}
REMOTE_CPP = {"mul": "/tmp/cand_mul.cpp", "div": "/tmp/cand_div.cpp", "add": "/tmp/cand_add.cpp"}
REMOTE_BIN = {"mul": "/tmp/cand_mul", "div": "/tmp/cand_div", "add": "/tmp/cand_add"}

VERIFY = r"""
import subprocess, glob, os
modes = {"mul": "/tmp/cand_mul", "div": "/tmp/cand_div", "add": "/tmp/cand_add"}
data = os.path.expanduser("~/hexbench/data")
for mode, binp in modes.items():
    indir = os.path.join(data, mode)
    cases = sorted(glob.glob(indir + "/*.in"))
    ok = bad = 0
    for f in cases:
        base = os.path.basename(f)[:-3]
        expf = os.path.join(indir, base + ".exp")
        explines = [l.strip() for l in open(expf).read().split("\n") if l.strip()]
        data_in = open(f).read()
        got = subprocess.run([binp], input=data_in, capture_output=True, text=True).stdout
        gotlines = [l.strip() for l in got.split("\n") if l.strip()]
        if gotlines == explines:
            ok += 1
        else:
            bad += 1
            print("BAD", mode, base, "expN", len(explines), "gotN", len(gotlines))
            for i in range(min(len(explines), len(gotlines))):
                if explines[i] != gotlines[i]:
                    print("  diff@%d exp=%s got=%s" % (i, explines[i][:40], gotlines[i][:40]))
                    break
    print("RESULT %s ok=%d bad=%d total=%d" % (mode, ok, bad, len(cases)))
"""

with open(r"D:\hex_precious_speed\tools\_verify_candidates.py", "w") as fp:
    fp.write(VERIFY)

builds = []
for mode, lp in LOCAL.items():
    vm.put(lp, REMOTE_CPP[mode])
    builds.append("g++ -O2 -march=x86-64-v3 -std=c++17 -o %s %s 2>&1; echo BUILD_%s=$?" %
                  (REMOTE_BIN[mode], REMOTE_CPP[mode], mode))
vm.put(r"D:\hex_precious_speed\tools\_verify_candidates.py", "/tmp/verify_candidates.py")

print("=== build ===")
print(vm.run(" && ".join(builds)))
print("=== verify ===")
print(vm.run("cd /tmp && python3 verify_candidates.py 2>&1; echo VERIFY=$?"))
