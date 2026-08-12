# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H = "/home/azzr/hexbench"
vmctl.run(f"cd {H} && perf record -q -F 8000 -e instructions:u -o /tmp/pm.data build/v16 < data/_big/mix3_big.in > /dev/null 2>&1; "
          f"perf report -i /tmp/pm.data --stdio --sort symbol 2>/dev/null | head -22")
vmctl.run(f"cd {H} && perf record -q -F 8000 -e instructions:u -o /tmp/pp.data build/v16 < data/_big/pow2_big.in > /dev/null 2>&1; "
          f"perf report -i /tmp/pp.data --stdio --sort symbol 2>/dev/null | head -18")
