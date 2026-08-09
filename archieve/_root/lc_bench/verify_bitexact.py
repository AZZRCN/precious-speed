"""逐字节比对两个 MUL 二进制在全部本地用例上的输出。

用法: python verify_bitexact.py <ref.exe> <new.exe> [casedir]
输出每个用例的 OK/DIFF，并断言输出非空（防止两边同时崩溃造成假阳性）。
"""
import hashlib
import pathlib
import subprocess
import sys

ref = sys.argv[1]
new = sys.argv[2]
casedir = pathlib.Path(sys.argv[3] if len(sys.argv) > 3 else "cases/mul")

cases = sorted(casedir.glob("*.in"))
if not cases:
    sys.exit(f"no cases in {casedir}")


def run(binary, case):
    with open(case, "rb") as fh:
        proc = subprocess.run([binary], stdin=fh, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL)
    return proc.returncode, proc.stdout


bad = 0
empty = 0
for case in cases:
    rc_a, out_a = run(ref, case)
    rc_b, out_b = run(new, case)
    ha = hashlib.md5(out_a).hexdigest()[:12]
    hb = hashlib.md5(out_b).hexdigest()[:12]
    tag = "OK  "
    if not out_a:
        tag, empty = "EMPTY", empty + 1
    elif out_a != out_b:
        tag, bad = "DIFF", bad + 1
    print(f"{tag} {case.name:<20} rc={rc_a}/{rc_b} bytes={len(out_a)}/{len(out_b)} "
          f"md5={ha}/{hb}")

print(f"=== cases={len(cases)} diff={bad} empty={empty} ===")
sys.exit(1 if (bad or empty) else 0)
