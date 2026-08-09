"""VM 端：将各候选输出与 gold 逐字节比对（20 个 GEN 用例）。"""
import hashlib
import os
import subprocess
import sys

CASES_DIR = "/home/azzr/mulbench/cases"
BIN_DIR = "/home/azzr/mulbench/bin"
REF = "v3_gold"
CANDS = sys.argv[1:] or ["v3_L19", "v3_L14", "v3_L12", "v3_L11", "v3_L10", "v3_L9"]

cases = sorted(f for f in os.listdir(CASES_DIR) if f.endswith(".in"))


def digest(binname, case):
    with open(os.path.join(CASES_DIR, case), "rb") as fh:
        proc = subprocess.run([os.path.join(BIN_DIR, binname)], stdin=fh,
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return proc.returncode, hashlib.md5(proc.stdout).hexdigest(), len(proc.stdout)


ref_digests = {}
for case in cases:
    rc, md5, size = digest(REF, case)
    ref_digests[case] = (md5, size)
    if rc != 0 or size == 0:
        print(f"!! REF FAILED on {case}: rc={rc} size={size}")

bad = 0
for cand in CANDS:
    mism = []
    for case in cases:
        rc, md5, size = digest(cand, case)
        if (md5, size) != ref_digests[case]:
            mism.append(case)
    if mism:
        bad += 1
        print(f"{cand:10} MISMATCH on {len(mism)} case(s): {mism[:4]}")
    else:
        print(f"{cand:10} bit-exact on all {len(cases)} cases")

print(f"VERIFY_DONE bad={bad}")
