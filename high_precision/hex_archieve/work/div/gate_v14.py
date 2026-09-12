import os, subprocess, sys

CASEDIR = "hexbench/data/div"
ok = bad = 0
failures = []
for f in sorted(os.listdir(CASEDIR)):
    if not f.endswith(".in"):
        continue
    inp = os.path.join(CASEDIR, f)
    exp = inp[:-3] + ".exp"
    r = subprocess.run([sys.executable, "divbench/hexcheck.py", "run",
                        "./div_v14", inp, exp],
                       capture_output=True, text=True)
    line = (r.stdout + r.stderr).strip().replace("\n", " ")
    if "OK" in line:
        ok += 1
    else:
        bad += 1
        failures.append(f + "=" + line[:80])
print("OK=%d BAD=%d" % (ok, bad))
if failures:
    print("FAILURES:")
    for x in failures:
        print("  " + x)
