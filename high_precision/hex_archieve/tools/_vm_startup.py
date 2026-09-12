# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
H="/home/azzr/hexbench"
vmctl.run(f"cd {H} && perf record -q -F 20000 -e instructions:u -o /tmp/p16.data build/v16nm < data/mul/example_00.in > /dev/null 2>&1; perf report -i /tmp/p16.data --stdio --sort symbol 2>/dev/null | head -25")
vmctl.run(f"cd {H} && perf record -q -F 20000 -e instructions:u -o /tmp/p13.data build/v13 < data/mul/example_00.in > /dev/null 2>&1; perf report -i /tmp/p13.data --stdio --sort symbol 2>/dev/null | head -25")
