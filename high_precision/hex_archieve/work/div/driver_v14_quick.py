import sys, time
sys.path.insert(0, r"D:\hex_precious_speed\tools")
from vm import run

for c in ["length_ratio_integer_05", "burnikel_ziegler_bound_03", "length_ratio_integer_00", "example_00", "max_00"]:
    cmd = ("cd /home/azzr && timeout 30 python3 divbench/hexcheck.py run ./div_v14 "
           "hexbench/data/div/%s.in hexbench/data/div/%s.exp 2>&1; echo RC=$?" % (c, c))
    t0 = time.time()
    rc, out, err = run(cmd, timeout=45)
    dt = time.time() - t0
    print("[%5.1fs] %-26s rc=%s %s" % (dt, c, rc, out.strip().replace(chr(10), " ")[:80]), flush=True)
