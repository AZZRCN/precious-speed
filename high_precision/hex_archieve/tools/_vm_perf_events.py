# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
vmctl.run("perf list hw cache 2>/dev/null | head -50")
vmctl.run("cd /home/azzr/hexbench && perf stat -e instructions:u,cycles:u,L1-dcache-loads,L1-dcache-load-misses,cache-references,cache-misses,branch-misses build/v13 < data/mul/max_max_01.in > /dev/null")
