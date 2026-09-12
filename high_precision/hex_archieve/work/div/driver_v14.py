import sys, time
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

# fetch case list
_, lst, _ = run("cd /home/azzr && ls hexbench/data/div/*.in")
cases = sorted(x.strip().split("/")[-1][:-3] for x in lst.strip().split() if x.strip().endswith(".in"))
print("num cases:", len(cases), file=sys.stderr)

ok = bad = 0
for c in cases:
    cmd = ("cd /home/azzr && timeout 60 python3 divbench/hexcheck.py run ./div_v14 "
           "hexbench/data/div/%s.in hexbench/data/div/%s.exp 2>&1; echo RC=$?" % (c, c))
    t0 = time.time()
    rc, out, err = run(cmd, timeout=90)
    dt = time.time() - t0
    line = out.strip().replace("\n", " ")
    if "OK" in line:
        ok += 1
        print("[OK ] %-28s %.1fs %s" % (c, dt, line[:60]))
    else:
        bad += 1
        print("[BAD] %-28s %.1fs rc=%s %s" % (c, dt, rc, line[:90]))
print("SUMMARY OK=%d BAD=%d" % (ok, bad))
