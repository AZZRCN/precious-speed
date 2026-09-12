import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
for b in ("v13w","v16","v18"):
    rc,o,_ = vmctl.run(f"grep -E 'refs:|misses:' /tmp/c_{b}_pow2_big.log | sed 's/==[0-9]*== //'")
    print(f"--- {b} pow2_big ---"); print(o)
