# AZZRCN
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vmctl
HERE = os.path.dirname(os.path.abspath(__file__))
vmctl.put(os.path.join(HERE, "casedist.py"), "/home/azzr/hexbench/casedist.py")
vmctl.run("cd /home/azzr/hexbench && python3 casedist.py data/mul max_max_00 max_max_01 fft_killer_00 fft_killer_01 large_00 large_01 large_02 large_small_00", timeout=600)
